#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "error_status.h"
#include "log.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

traffic_signal_status_t signal_status;
traffic_signal_status_t current_signal_status;
pthread_mutex_t mutex_signal_status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_current_signal_status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_compensation = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_signal_status;
static uint16_t pretime_sent_count = 0;
extern uint8_t flag_pretime;
extern uint8_t flag_switch2nextStep;
extern uint8_t flag_PhaseOrder;
uint8_t phase_change_flag = false;
uint8_t real_pretime = 0;
static uint8_t FirstSwitchFlag = true;
static uint8_t PhaseOrder_initial = true;

// static bool flag = true;
extern int16_t compensation_buffer[SUBPHASEID_NUM];

/* 5F CC 回報時相步階 */
void packet_5FCC(traffic_signal_packet_t *packet)
{
    static uint8_t previous_phase = 0;
    // static uint8_t init_get_phase_flag=false;

    pthread_mutex_lock(&mutex_signal_status);

    signal_status.ControlStrategy = packet->INFO[2];
    // here is to map 成龍 controlstrategy to be same as 山住;0/1 成龍/三住
    if (config.signal_controller_manufacturer == 0 &&
        signal_status.ControlStrategy == 21) {
        log_file_write(
            "map cheng_long dynamic controlstrategy from 21 to 16\n\r");
        signal_status.ControlStrategy = 16;
    } else if (config.signal_controller_manufacturer == 0 &&
               signal_status.ControlStrategy == 5) {
        log_file_write(
            "map cheng_long pretime controlstrategy from 5 to 1\n\r");
        signal_status.ControlStrategy = 1;
    } else {
        log_file_write("no control strategy map action should be taken\n\r");
    }


    // for some error situation happens in CHENG_LONG
    if (packet->INFO[3] != 0 && packet->INFO[4] != 5) {
        signal_status.SubPhaseID = packet->INFO[3];
    }
    signal_status.StepID = packet->INFO[4];
    signal_status.StepSec = (packet->INFO[5] << 8) | packet->INFO[6];

    // phase change happen!!

    // printf("previous phase is %d and current phase is %d\r\n",
    // previous_phase, signal_status.SubPhaseID);
    if (previous_phase != signal_status.SubPhaseID) {
        if (FirstSwitchFlag == true) {  // 第一次換相不取值
            FirstSwitchFlag = false;
        } else {
            signal_status.plan[signal_status.SubPhaseID - 1]
                .PreTimeCompensated = signal_status.StepSec;
            printf("phase changed and pretime for phase %d is %d\r\n",
                   signal_status.SubPhaseID, signal_status.StepSec);
        }
    }


    previous_phase = signal_status.SubPhaseID;



    if (signal_status.StepSec > 255 && signal_status.StepID == 1) {
        flag_switch2nextStep = true;
        set_655xx_error();
    }
    if (signal_status.StepSec <= 255 && signal_status.StepID == 1) {
        clear_655xx_error();
    }

    // execute pretime instruction to force tc go back to pretime
    // to prevent the tc not go back to pretime after 全動態
    // pretime_sent_count is for let pretime sent one time only in step 4
    // now, have to check command buffer whether or not is empty
    // if it is empty and return pretime control status.
    if (signal_status.StepID == 4) {  // 4 閃黃燈
        // printf("command_buf_empty:%d\r\n",check_command_buf_empty());
        if (pretime_sent_count == 0 && check_command_buf_empty()) {
            log_file_write("5FCC: execute go back to pretime at step 4\r\n");
            printf("5FCC updating: set pretime!\r\n");
            // tsc_pretime();
            flag_pretime = true;
        }
        pretime_sent_count++;
    } else {
        pretime_sent_count = 0;
    }


    if (config.log_signal_packet_info) {
        log_file_write("signal packet info: 5FCC\nControlStrategy: %d\nSubPhaseID: %d\nStepID: %d\nStepSec: %d",
                       signal_status.ControlStrategy, signal_status.SubPhaseID, signal_status.StepID, signal_status.StepSec);
    }
    pthread_mutex_unlock(&mutex_signal_status);

    int sem_value;
    // sem初始值為1 所以0代表只有自己近來的狀況嗎？
    sem_getvalue(&sem_signal_status, &sem_value);
    if (sem_value == 0) {
        sem_post(&sem_signal_status);
    }

    clear_tsc_5fcc_error();
    return;
}

/* 5F C8 查詢目前時制計劃內容（非控制器內時制資料庫內容） */
void packet_5FC8(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_current_signal_status);

    signal_status.PlanID = packet->INFO[2];

    current_signal_status.PhaseOrder = packet->INFO[4];
    current_signal_status.SubPhaseCount = packet->INFO[5];
    // printf("current_signal_status.SubPhaseCount:%d\r\n",current_signal_status.SubPhaseCount);
    for (int i = 0; i < current_signal_status.SubPhaseCount; i++) {
        current_signal_status.plan[i].Green =
            (packet->INFO[6 + i * 2] << 8 | packet->INFO[7 + i * 2]);
    }
    current_signal_status.CycleTime =
        packet->INFO[6 + current_signal_status.SubPhaseCount * 2] << 8 |
        packet->INFO[7 + current_signal_status.SubPhaseCount * 2];
    current_signal_status.Offset =
        packet->INFO[8 + current_signal_status.SubPhaseCount * 2] << 8 |
        packet->INFO[9 + current_signal_status.SubPhaseCount * 2];
    // if(flag == true){
    //     get_compensation_buffer(compensation_buffer);
    //     flag = false;
    // }
    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "signal packet info: 5FC8");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nPlanID: %d",
                 signal_status.PlanID);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nPhaseOrder: %d",
                 current_signal_status.PhaseOrder);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseCount: %d",
                 current_signal_status.SubPhaseCount);
        for (int i = 0; i < current_signal_status.SubPhaseCount; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\nGreen: %d",
                     current_signal_status.plan[i].Green);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nCycleTime: %d",
                 current_signal_status.CycleTime);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nOffset: %d",
                 current_signal_status.Offset);
        log_file_write(log_content);
    }

    pthread_mutex_unlock(&mutex_current_signal_status);
    return;
}
/* 5F C5 回報時制計畫編號與資料庫 */  // 收tc箱資料
void packet_5FC5(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);
    // signal_status.PlanID = packet->INFO[2];
    //
    if (PhaseOrder_initial == true) {
        flag_PhaseOrder = true;
        PhaseOrder_initial = false;
    }
    if (signal_status.PhaseOrder != packet->INFO[4]) {
        command_buf_clear();
        flag_PhaseOrder = true;
    }
    signal_status.PhaseOrder = packet->INFO[4];
    signal_status.SubPhaseCount = packet->INFO[5];
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        signal_status.plan[i].Green =
            (packet->INFO[6 + i * 2] << 8 | packet->INFO[7 + i * 2]);
    }
    signal_status.CycleTime = packet->INFO[6 + signal_status.SubPhaseCount * 2]
                                  << 8 |
                              packet->INFO[7 + signal_status.SubPhaseCount * 2];
    signal_status.Offset = packet->INFO[8 + signal_status.SubPhaseCount * 2]
                               << 8 |
                           packet->INFO[9 + signal_status.SubPhaseCount * 2];

    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "signal packet info: 5FC5");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nPlanID: %d",
                 signal_status.PlanID);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nPhaseOrder: %d",
                 signal_status.PhaseOrder);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseCount: %d",
                 signal_status.SubPhaseCount);
        for (int i = 0; i < signal_status.SubPhaseCount; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\nGreen: %d",
                     signal_status.plan[i].Green);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nCycleTime: %d",
                 signal_status.CycleTime);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nOffset: %d",
                 signal_status.Offset);
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

static uint8_t initialize_flag = true;
static uint8_t count_initialize = 0;
/* 5F C4 回報時制計畫基本參數 */
void packet_5FC4(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);
    signal_status.SubPhaseCount = packet->INFO[3];
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        signal_status.plan[i].MinGreen = packet->INFO[4 + i * 7];
        signal_status.plan[i].MaxGreen =
            (packet->INFO[5 + i * 7] << 8 | packet->INFO[6 + i * 7]);
        signal_status.plan[i].Yellow = packet->INFO[7 + i * 7];
        signal_status.plan[i].AllRed = packet->INFO[8 + i * 7];
        signal_status.plan[i].PedGreenFlash = packet->INFO[9 + i * 7];
        signal_status.plan[i].PedRed = packet->INFO[10 + i * 7];

        signal_status.plan[i].PreGreen =
            signal_status.plan[i].Green - signal_status.plan[i].PedGreenFlash;

        // 為了初始化被補償的pretime
        if (initialize_flag == true) {
            count_initialize++;
            // printf("get inside\r\n");
            if (count_initialize >
                signal_status.SubPhaseCount * 2) {
                // 2是為了第一次讀出來的值常常是錯誤的
                // 所以等到第二次讀取才取值

                signal_status.plan[i].PreTimeCompensated =
                    signal_status.plan[i].PreGreen;
                // printf("phase %d is %d\r\n", i+1,
                // signal_status.plan[i].PreTimeCompensated);
            }
        }
    }
    if (initialize_flag == true &&
        count_initialize > signal_status.SubPhaseCount * 2) {
        initialize_flag = false;
        count_initialize = 0;
    }


    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "signal packet info: 5FC4");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseCount: %d",
                 signal_status.SubPhaseCount);
        for (int i = 0; i < signal_status.SubPhaseCount; i++) {
            snprintf(
                log_content + strlen(log_content),
                LOG_CONTENT_LEN - strlen(log_content),
                "\nstatus.plan[%d] G:%2d minG:%2d maxG:%3d Y:%1d AR:%1d PG:%1d "
                "PR:%1d PreG:%2d",
                i, signal_status.plan[i].Green, signal_status.plan[i].MinGreen,
                signal_status.plan[i].MaxGreen, signal_status.plan[i].Yellow,
                signal_status.plan[i].AllRed,
                signal_status.plan[i].PedGreenFlash,
                signal_status.plan[i].PedRed, signal_status.plan[i].PreGreen);
        }
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

void packet_5FC3(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);

    signal_status.SignalMap = packet->INFO[3];
    signal_status.SignalCount = packet->INFO[4];

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\nSignalMap:%x SignalCount:%x\r\n",
             signal_status.SignalMap, signal_status.SignalCount);

    printf("SignalMap:%x SignalCount:%x\r\n", signal_status.SignalMap, signal_status.SignalCount);


    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for (int j = 0; j < signal_status.SignalCount; j++) {
            signal_status.phaseorder_plan[i][j].SignalStatus = packet->INFO[6 + i * signal_status.SignalCount + j];
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "SignalStatus:%x ",
                     signal_status.phaseorder_plan[i][j].SignalStatus);
            printf("SignalStatus:%x ", signal_status.phaseorder_plan[i][j].SignalStatus);
        }
        printf("\r\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\r\n");
    }

    log_file_write(log_content);

    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

void packet_5F0C(traffic_signal_packet_t *packet) {}

void packet_0FC2(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);

    signal_status.Year = packet->INFO[2];
    signal_status.Month = packet->INFO[3];
    signal_status.Day = packet->INFO[4];
    signal_status.Week = packet->INFO[5];
    signal_status.Hour = packet->INFO[6];
    signal_status.Min = packet->INFO[7];
    signal_status.Sec = packet->INFO[8];

    // printf("%d/%d/%d/%d %d:%d:%d\r\n",signal_status.Year,
    //                                 signal_status.Month,
    //                                 signal_status.Day,
    //                                 signal_status.Week,
    //                                 signal_status.Hour,
    //                                 signal_status.Min,
    //                                 signal_status.Sec);

    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

void packet_0F04(traffic_signal_packet_t *packet)
{
    // printf("tc status info: ");
    pthread_mutex_lock(&mutex_signal_status);

    uint16_t original_tc_hstatus = packet->INFO[2] << 8 | packet->INFO[3];
    signal_status.original_tc_health_status = original_tc_hstatus;
    log_file_write("original_tc_health_status is %04X\n\r", original_tc_hstatus);
    // dont show bit 14, 8, 9 for they seprately means controller ready,
    // cabinated opened, communication connect
    // original_tc_hstatus=original_tc_hstatus&0xbcff;
    // 1001 1101 0001 0011
    original_tc_hstatus =
        original_tc_hstatus &
        0x9d13;  // 介庸學長建議如下
                 //  Bit0、1、4、8、10、11、12、15要通報處理，因為控制不是無法控制就是故障不亮或跳閃光模式

    printf("tc status\n\r");
    printf("%04X\n\r", original_tc_hstatus);

    log_file_write("tc_health_status after mask is %04X\n\r", original_tc_hstatus);

    pthread_mutex_unlock(&mutex_signal_status);
}

// 裡面有些部份看不太懂 為何要用號誌加上mutex保護
void get_traffic_signal_status(traffic_signal_status_t *traffic_signal_status)
{  // wait what??
    sem_timedwait_millsecs(&sem_signal_status, SEM_SIGNAL_STATUS_TIMEOUT);

    pthread_mutex_lock(&mutex_signal_status);
    // signal_status global variable and it's why use mutex to protect
    memcpy(traffic_signal_status, &signal_status,
           sizeof(traffic_signal_status_t));
    pthread_mutex_unlock(&mutex_signal_status);

    // 號誌的釋放
    int sem_value;
    sem_getvalue(&sem_signal_status, &sem_value);
    if (sem_value == 0) {
        sem_post(&sem_signal_status);
    }
    return;
}

void get_current_traffic_signal_status(
    traffic_signal_status_t *traffic_signal_status)
{
    pthread_mutex_lock(&mutex_current_signal_status);
    memcpy(traffic_signal_status, &current_signal_status,
           sizeof(traffic_signal_status_t));
    pthread_mutex_unlock(&mutex_current_signal_status);
    return;
}

uint8_t get_current_phase()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t phase = signal_status.SubPhaseID;
    pthread_mutex_unlock(&mutex_signal_status);
    return phase;
}

uint8_t get_current_step()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t step = signal_status.StepID;
    pthread_mutex_unlock(&mutex_signal_status);
    return step;
}

uint16_t get_current_second()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint16_t second = signal_status.StepSec;
    pthread_mutex_unlock(&mutex_signal_status);
    return second;
}

uint8_t get_SubPhaseCount()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint16_t SubPhaseCount = signal_status.SubPhaseCount;
    pthread_mutex_unlock(&mutex_signal_status);
    return SubPhaseCount;
}

uint8_t get_SignalCount()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint16_t SignalCount = signal_status.SignalCount;
    pthread_mutex_unlock(&mutex_signal_status);
    return SignalCount;
}

uint8_t get_plan_id()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t plan_id = signal_status.PlanID;
    pthread_mutex_unlock(&mutex_signal_status);
    return plan_id;
}

uint8_t get_control_status()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t control_status = signal_status.control_status;
    pthread_mutex_unlock(&mutex_signal_status);
    return control_status;
}
uint16_t get_original_tc_health_status()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint16_t original_tc_health_status = signal_status.original_tc_health_status;
    pthread_mutex_unlock(&mutex_signal_status);
    return original_tc_health_status;
}
// 不同的step進來看到的remaining time不一樣 用自己剩餘的秒數
// 在加上還沒跑得step的秒數 就是remaining time
uint16_t get_remaining_time(uint8_t phase, uint8_t step, uint16_t second)
{
    pthread_mutex_lock(&mutex_signal_status);
    switch (step) {
    case 1:
        second = second + signal_status.plan[phase - 1].PedGreenFlash +
                 signal_status.plan[phase - 1].Yellow +
                 signal_status.plan[phase - 1].PedRed +
                 signal_status.plan[phase - 1].AllRed;
        break;
    case 2:
        second = second + signal_status.plan[phase - 1].Yellow +
                 signal_status.plan[phase - 1].PedRed +
                 signal_status.plan[phase - 1].AllRed;
        break;
    case 3:
        second = second + signal_status.plan[phase - 1].PedRed +
                 signal_status.plan[phase - 1].AllRed;
        break;
    case 4:
        second = second + signal_status.plan[phase - 1].AllRed;
        break;
    case 5:
        break;

    default:
        break;
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return second;
}

uint8_t get_next_SubPhaseID()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t phase_count = signal_status.SubPhaseCount;
    uint8_t phase = signal_status.SubPhaseID;
    pthread_mutex_unlock(&mutex_signal_status);

    if (phase == phase_count) {
        return 1;
    } else {
        return phase + 1;
    }
}

uint8_t get_prev_SubPhaseID()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t phase_count = signal_status.SubPhaseCount;
    uint8_t phase = signal_status.SubPhaseID;
    pthread_mutex_unlock(&mutex_signal_status);
    uint8_t prev = phase - 1;
    if (prev == 0)
        return phase_count;
    else
        return prev;
}

uint8_t get_PhaseOrder()
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t PhaseOrder = signal_status.PhaseOrder;
    pthread_mutex_unlock(&mutex_signal_status);
    return PhaseOrder;
}

// SubPhaseCount_index:第幾個 phase
// SignalCount_index: 第幾個岔路口 ; 北邊 index 為 0
uint8_t get_SignalStatus(uint8_t SubPhaseCount_index, uint8_t SignalCount_index)
{
    pthread_mutex_lock(&mutex_signal_status);
    uint8_t SignalStatus = signal_status.phaseorder_plan[SubPhaseCount_index][SignalCount_index].SignalStatus;
    pthread_mutex_unlock(&mutex_signal_status);
    return SignalStatus;
}

int16_t get_total_compensation_second()
{
    pthread_mutex_lock(&mutex_compensation);
    int16_t total_compensation_second = 0;
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        total_compensation_second += compensation_buffer[i];
    }
    pthread_mutex_unlock(&mutex_compensation);
    return total_compensation_second;
}

void get_compensation_buffer(int16_t *compensation_buffer)
{
    pthread_mutex_lock(&mutex_compensation);
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        compensation_buffer[i] =
            current_signal_status.plan[i].Green - signal_status.plan[i].Green;
    }
    pthread_mutex_unlock(&mutex_compensation);
    return;
}

void set_control_status(uint8_t control_status)
{
    pthread_mutex_lock(&mutex_signal_status);
    signal_status.control_status = control_status;
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

// 這個函式在幹麻？？ 要廣播給obu現在tc箱的狀況
void report_plan()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2V_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("report_plan: malloc");
        perror("report_plan: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2V_SPECIFIC_FIELD_MAX_LEN);
    }

    pthread_mutex_lock(&mutex_signal_status);
    // control signal_status
    write_uint8_t(signal_status.control_status, &write_buf);
    // dynamic plan
    write_uint8_t(signal_status.SubPhaseID, &write_buf);
    write_uint8_t(signal_status.StepID, &write_buf);
    write_uint16_t(signal_status.StepSec, &write_buf);

    // phase order
    write_uint8_t(signal_status.PhaseOrder, &write_buf);
    // plan id
    write_uint8_t(signal_status.PlanID, &write_buf);
    // cycle
    write_uint16_t(signal_status.CycleTime, &write_buf);
    // offset
    write_uint16_t(signal_status.Offset, &write_buf);
    // subphase count
    write_uint8_t(signal_status.SubPhaseCount, &write_buf);
    // static plan
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        write_uint8_t(i + 1, &write_buf);  // subphase id
        write_uint16_t(signal_status.plan[i].Green, &write_buf);
        write_uint8_t(signal_status.plan[i].Yellow, &write_buf);
        write_uint8_t(signal_status.plan[i].AllRed, &write_buf);
        write_uint8_t(signal_status.plan[i].PedGreenFlash, &write_buf);
        write_uint8_t(signal_status.plan[i].PedRed, &write_buf);
        write_uint16_t(signal_status.plan[i].MaxGreen, &write_buf);
        write_uint8_t(signal_status.plan[i].MinGreen, &write_buf);
    }
    pthread_mutex_unlock(&mutex_signal_status);

    OBU_packet_tx(write_buf.index, 0, write_buf.content);
    free(write_buf.content);
    return;
}

/*****************************************************************************
** Function:    sem_timedwait_millsecs
** Description: semaphore wait a limit on the amount of time specified in
*milliseconds.
** Parameter:   sem: semaphore handle
**              msecs: timeout value
** Return:      none
** Reference:   https://blog.csdn.net/wy5761/article/details/9320331
******************************************************************************/
void sem_timedwait_millsecs(sem_t *sem, long msecs)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long secs = msecs / 1000;
    msecs = msecs % 1000;

    long add = 0;
    msecs = msecs * 1000 * 1000 + ts.tv_nsec;
    add = msecs / (1000 * 1000 * 1000);
    ts.tv_sec += (add + secs);
    ts.tv_nsec = msecs % (1000 * 1000 * 1000);

    if (sem_timedwait(sem, &ts) == -1) {
        log_file_write_fatal_error("sem_timedwait_millsecs: sem_timedwait (%d)",
                                   errno);
        perror("sem_timedwait_millsecs: sem_timedwait");
    } else {
        return;
    }
}
