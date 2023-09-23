#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "common_packet_tx.h"
#include "config.h"
#include "log.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

#define ArgTrafficStatus traffic_signal_status_t *signal_status
#define ArgLogContent char log_content[LOG_CONTENT_LEN + 1]
#define ArgLogAndStatus ArgTrafficStatus, ArgLogContent
#define PassLogAndStaus &signal_status, log_content

pthread_mutex_t mutex_compensation = PTHREAD_MUTEX_INITIALIZER;
int16_t compensation_buffer[SUBPHASEID_NUM] = {0};  // 儲存有被延長或是縮短過的秒數

static inline void get_compensation_buffer(int16_t buf[SUBPHASEID_NUM])
{
    pthread_mutex_lock(&mutex_compensation);
    memcpy(buf, compensation_buffer, sizeof(compensation_buffer));
    pthread_mutex_unlock(&mutex_compensation);
}

void set_compensation_buffer(int subphaseId, int adjust_time)
{
    pthread_mutex_lock(&mutex_compensation);
    compensation_buffer[subphaseId - 1] += adjust_time;
    pthread_mutex_unlock(&mutex_compensation);
}

void compensation_buffer_clear()
{
    pthread_mutex_lock(&mutex_compensation);
    memset(compensation_buffer, 0, sizeof(compensation_buffer));
    pthread_mutex_unlock(&mutex_compensation);
    printf("\r\ncompensation_buffer is cleared\r\n");
    log_file_write("compensation_buffer is cleared\r\n");
}

uint8_t is_in_compensation()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    uint8_t current_phase = get_current_phase();

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    // 只能那個step 的起始秒數去跟plan pretime 比較
    // 如果不一樣就是在補償
    // 因為補償是下個周期去對齊起始點不一樣就補償
    // 準備要補償時，換相那瞬間的秒數會被更新到PreTimeCompensated
    // 所以當PreTimeCompensated ！= pregreen就是代表正在補償
    if (signal_status.ControlStrategy == 1) {
        if (signal_status.plan[current_phase - 1].PreTimeCompensated > 0) {
            if (signal_status.plan[current_phase - 1].PreGreen !=
                signal_status.plan[current_phase - 1].PreTimeCompensated) {
                snprintf(
                    log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "signal_status.plan[%d].PreTimeCompensated:%d\r\n",
                    current_phase - 1, signal_status.plan[current_phase - 1].PreTimeCompensated);
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "signal_status.plan[%d].PreGreen:%d\r\n",
                         current_phase - 1,
                         signal_status.plan[current_phase - 1].PreGreen);
                log_file_write(log_content);
                printf("do compensation\r\n");
                log_file_write("do compensation\r\n");
                return true;  // 正在補償
            } else {
                printf("No compensation\r\n");
                log_file_write("No compensation\r\n");
                return false;
            }
        } else
            return false;
    } else {
        for (int i = 0; i < CYCLE_NUM; i++) {
            for (int j = 0; j < SUBPHASEID_NUM; j++)
                if (command_buf[i][j].app_id == COMPENSATION_ID)
                    return true;
        }
        return false;
    }
}

// 取得進行零時零分基準點補償與現在補償的差距
static inline int16_t get_alignment_compensation_time(ArgTrafficStatus, int alignHour, int alignMin)
{
    const uint32_t daySec = 86400;
    uint16_t cycleTime, offset, subPhaseID;
    int secInDay, compTime = 0;
    struct timeval tv;
    struct tm timeinfo;

    cycleTime = signal_status->CycleTime;
    offset = signal_status->Offset;
    subPhaseID = signal_status->SubPhaseID - 1;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    // 減掉時差 與 加上 與 tc 時間的誤差
    secInDay = ((timeinfo.tm_hour - alignHour) * 60 + (timeinfo.tm_min - alignMin)) * 60 +
               timeinfo.tm_sec - offset + signal_status->tcTimeOffest;
    // 要對齊第一個時向的第一個步階的第一秒
    // 先扣掉經過的時向
    for (int i = 0; i < subPhaseID; i++) {
        secInDay -= (signal_status->plan[i].PreTimeCompensated + signal_status->plan[i].PedGreenFlash +
                     signal_status->plan[i].PedRed + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed);
    }
    // 再扣除經過的步階
    switch (signal_status->StepID) {
    case 5:
        secInDay -= signal_status->plan[subPhaseID].Yellow;
    case 4:
        secInDay -= signal_status->plan[subPhaseID].PedRed;
    case 3:
        secInDay -= signal_status->plan[subPhaseID].PedGreenFlash;
    case 2:
        secInDay -= signal_status->plan[subPhaseID].PreTimeCompensated;
    default:
        break;
    }
    // 最後扣除清過的秒數
    if (signal_status->StepID == 1)
        secInDay -= signal_status->plan[subPhaseID].PreTimeCompensated;
    else
        signal_status->plan[subPhaseID].StepArr[signal_status->StepID - 1];
    secInDay += signal_status->StepSec;

    // 避免是凌晨 0 點扣到變成負的 (前一天)
    secInDay = (secInDay + daySec) % daySec;  // 絕對值
    compTime = secInDay % cycleTime;

    // 小於 cycleTime 的 1/2 就用扣的 大於就用加的去對齊
    return (cycleTime / 2) < compTime ? -compTime : cycleTime - compTime;
}

static inline int16_t get_total_compensation_second_with_status(ArgTrafficStatus)
{
    if (config.signal_controller_manufacturer == CHENG_LONG) {
        struct timeval tv;
        struct tm timeinfo;

        gettimeofday(&tv, NULL);
        localtime_r(&tv.tv_sec, &timeinfo);

        int MinInDay = timeinfo.tm_hour * 60 + timeinfo.tm_min;
        for (int i = 1; i < signal_status->SegmentCount; i++) {
            if (MinInDay < (signal_status->allday_plan[i].Hour * 60 + signal_status->allday_plan[i].Min)) {
                return get_alignment_compensation_time(signal_status, signal_status->allday_plan[i - 1].Hour, signal_status->allday_plan[i - 1].Min);
            }
        }
        return get_alignment_compensation_time(signal_status, signal_status->allday_plan[signal_status->SegmentCount - 1].Hour, signal_status->allday_plan[signal_status->SegmentCount - 1].Min);
    } else {
        return get_alignment_compensation_time(signal_status, 0, 0);
    }
}

int16_t get_total_compensation_second()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    return get_total_compensation_second_with_status(&signal_status);
}

static inline void insert_compensation_command(ArgLogAndStatus, int Comp_cyclenum, int cycle_index, uint16_t *subphase_compensation_time)
{
    tsc_command_t command = {0};
    int ret = 0, subPhaseID = signal_status->SubPhaseID - 1;
    int subphase_ptr = subPhaseID;

    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME));

    // 如果現在不是步階 1 或是步階 1 會調整到小於 MinGreen 或超過 MaxGreen 則到下一個 cycle
    if (signal_status->StepID != 1 ||
        signal_status->StepSec - subphase_compensation_time[subPhaseID] < signal_status->plan[subPhaseID].MinGreen ||
        signal_status->StepSec - subphase_compensation_time[subPhaseID] > signal_status->plan[subPhaseID].MaxGreen) {
        subphase_ptr = (subphase_ptr + 1) % signal_status->SubPhaseCount;
    }

    cycle_index %= CYCLE_NUM;
    command.compensation_cycle = Comp_cyclenum;
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        if (subphase_compensation_time[subphase_ptr] == 0)
            goto NotInsertCommand;

        command.cycle = cycle_index;
        command.target_phase = command.phase = subphase_ptr + 1;
        command.compensation_time = subphase_compensation_time[subphase_ptr];
        command.effect_time = signal_status->plan[subphase_ptr].PreTimeCompensated - subphase_compensation_time[subphase_ptr];

        printf("cycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)\r\n",
               command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
        ret = command_buf_insert_effect_time(&command);
        log_snprintf(log_content, "\ncycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)",
                     command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
    NotInsertCommand:
        subphase_ptr++;
        if (subphase_ptr >= signal_status->SubPhaseCount) {
            subphase_ptr %= signal_status->SubPhaseCount;
            cycle_index = (cycle_index + 1) % CYCLE_NUM;
        }
    }
}

// 按照比例把補償時間分配給這個 subphase 並回傳因最大綠或最小綠造成的剩餘時間
static inline int allocate_compensation_by_weight(ArgTrafficStatus, float phase_weight[PHASE_COUNT_MAX_NUM], int compensation_time, int16_t subphase_compensation_time[SUBPHASEID_NUM])
{
    int remaining_time = compensation_time;
    float weights[SUBPHASEID_NUM] = {0};

    for (int i = 0; i < signal_status->SubPhaseCount; i++)
        weights[i] = phase_weight[i] * 0.01;

    for (int i = 0; i < signal_status->SubPhaseCount && remaining_time != 0; i++) {
        int comp_time = ceil(compensation_time * weights[i]);                 // 取無條件進位
        comp_time = comp_time > remaining_time ? remaining_time : comp_time;  // 少於剩餘時間就等於剩餘時間

        int effect_time = signal_status->plan[i].PreTimeCompensated - subphase_compensation_time[i] - comp_time;
        if (effect_time < signal_status->plan[i].MinGreen)
            effect_time = signal_status->plan[i].MinGreen;
        else if (effect_time > signal_status->plan[i].MaxGreen)
            effect_time = signal_status->plan[i].MaxGreen;

        comp_time = signal_status->plan[i].PreTimeCompensated - subphase_compensation_time[i] - effect_time;
        subphase_compensation_time[i] += comp_time;
        remaining_time -= comp_time;
    }
    return remaining_time;
}

// 平均分配剩餘的補償時間
static inline void allocate_remaining_time(ArgTrafficStatus, int remaining_time, int16_t subphase_compensation_time[SUBPHASEID_NUM])
{
    int pre_remaining_time = 0;

    // 會持續分配到剩餘時間為 0 或是無法再分配了
    while (pre_remaining_time != remaining_time && remaining_time != 0) {
        float weight[PHASE_COUNT_MAX_NUM] = {0};
        uint8_t canAdjustNum = 0;
        // 如果剩餘時間大於 0 要檢查是否已經是 MinGreen 了 反之大於
        // 然後將還剩餘的時間分配給還可以調整的時向
        for (int i = 0; i < signal_status->SubPhaseCount; i++) {
            int effect_time = signal_status->plan[i].PreTimeCompensated - subphase_compensation_time[i];
            if (remaining_time > 0 && effect_time > signal_status->plan[i].MinGreen) {
                canAdjustNum++;
            }
            if (remaining_time < 0 && effect_time < signal_status->plan[i].MaxGreen) {
                canAdjustNum++;
            }
        }
        for (int i = 0; i < signal_status->SubPhaseCount; i++) {
            int effect_time = signal_status->plan[i].PreTimeCompensated - subphase_compensation_time[i];
            if (remaining_time > 0 && effect_time > signal_status->plan[i].MinGreen) {
                weight[i] = 100 / canAdjustNum;
            }
            if (remaining_time < 0 && effect_time < signal_status->plan[i].MaxGreen) {
                weight[i] = 100 / canAdjustNum;
            }
        }
        remaining_time = allocate_compensation_by_weight(signal_status, weight, remaining_time, subphase_compensation_time);
        pre_remaining_time = remaining_time;
    }
}

// 把總補償秒數平均分給各個週期
static inline int get_cycle_compensation_time(ArgLogAndStatus, int16_t cycle_compensations[CYCLE_NUM], uint8_t Comp_cyclenum, uint8_t methodId)
{
    int16_t total_compensation_time, tmp_comp;

    total_compensation_time = get_total_compensation_second_with_status(signal_status);  // 總補償秒數
    tmp_comp = total_compensation_time;

    // 平均的分配到每個周期 最後多的秒數加在第一個周期
    for (int i = 1; i < Comp_cyclenum; i++) {
        cycle_compensations[i] = tmp_comp / Comp_cyclenum;
        tmp_comp -= cycle_compensations[i];
    }
    cycle_compensations[0] = tmp_comp;

    log_snprintf(log_content, "start compensation %d \r\nTotal compensation second:%d\r\n compensation cycle is %d\r\n",
                 methodId, total_compensation_time, Comp_cyclenum);
    for (int i = 0; i < Comp_cyclenum; i++) {
        log_snprintf(log_content, "\ncompensation cycle %d is %d", i, cycle_compensations[i]);
    }
    // 如果補償時間為 0 不做事
    return total_compensation_time;
}

static inline int implement_compensation_by_weight(ArgLogAndStatus, int16_t cycle_compensations[CYCLE_NUM], uint8_t Comp_cyclenum, float phase_weight[PHASE_COUNT_MAX_NUM])
{
    int16_t subphase_compensation_time[SUBPHASEID_NUM];

    for (int i = 0; i < Comp_cyclenum; i++) {
        memset(subphase_compensation_time, 0, sizeof(subphase_compensation_time));
        uint16_t remaining_time =
            allocate_compensation_by_weight(signal_status, phase_weight, cycle_compensations[i], subphase_compensation_time);
        allocate_remaining_time(signal_status, remaining_time, subphase_compensation_time);
        insert_compensation_command(signal_status, log_content, Comp_cyclenum, i, subphase_compensation_time);
    }
}

// 原時向補償
static inline void traffic_compensation_method1(ArgLogContent, uint8_t Comp_cyclenum)
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    traffic_signal_status_t signal_status;

    float phase_weight[PHASE_COUNT_MAX_NUM] = {0};
    int16_t comp_buf[SUBPHASEID_NUM] = {0};
    int adjustNum = 0;

    get_traffic_signal_status(&signal_status);
    get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 2);

    // 查詢有哪些時向是被調整過的 並分配比例
    get_compensation_buffer(comp_buf);
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        if (comp_buf[i] != 0)
            adjustNum++;
    }
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        if (comp_buf[i] != 0)
            phase_weight[i] = 100.0 / adjustNum;
    }

    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

// 依照設定權重比例分配補長時間
static inline void traffic_compensation_method2(ArgLogContent, uint8_t Comp_cyclenum, float phase_weight[PHASE_COUNT_MAX_NUM])
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    traffic_signal_status_t signal_status;

    get_traffic_signal_status(&signal_status);
    get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 2);
    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

/***************
幹支道明顯的道路
 1. 延長延幹道
 2. 縮短縮支道
***************/
static inline void traffic_compensation_method3(ArgLogContent, uint8_t Comp_cyclenum)
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    int16_t total_compensation_time = 0;
    traffic_signal_status_t signal_status;

    uint8_t arterial_phase = 0, branch_phase = 255;
    float phase_weight[PHASE_COUNT_MAX_NUM] = {0};

    get_traffic_signal_status(&signal_status);
    total_compensation_time = get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 2);

    // 綠燈長度最長的為主幹道 第二長的為支道
    for (int i = 1; i < signal_status.SubPhaseCount; i++) {
        if (signal_status.plan[i].PreGreen > signal_status.plan[arterial_phase].PreGreen) {
            branch_phase = arterial_phase;
            arterial_phase = i;
        } else if (branch_phase == 255 || signal_status.plan[i].PreGreen > signal_status.plan[branch_phase].PreGreen) {
            branch_phase = i;
        }
    }

    log_snprintf(log_content, "arterial_phase:%d \r\n branch_phase:%d\r\n", arterial_phase + 1, branch_phase + 1);

    if (total_compensation_time > 0) {  // 進行負補償
        printf("minus compensation\r\n");
        log_snprintf(log_content, "minus compensation\r\n");
        phase_weight[branch_phase] = 100;
    } else if (total_compensation_time < 0) {  // 進行正補償
        printf("positive compensation\r\n");
        log_snprintf(log_content, "positive compensation\r\n");
        phase_weight[arterial_phase] = 100;
    }

    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

void start_compensation()
{
    char log_content[LOG_CONTENT_LEN + 1] = {0};

    pthread_mutex_lock(&mutex_compensation);
    for (int i = 0; i < SUBPHASEID_NUM; i++)
        log_snprintf(log_content, "compensation_buffer[%d]:%d\r\n", i, compensation_buffer[i]);
    pthread_mutex_unlock(&mutex_compensation);

    switch (config.traffic_compensation_method) {
    case 0:
        log_snprintf(log_content, "start compensation 0\r\n Do nothing. TC automatic compensation.\r\n");
        goto NotReportEnd;
        break;
    case 1:
        traffic_compensation_method1(log_content, config.traffic_compensation_cycle_number);
        break;
    case 2:
        traffic_compensation_method2(log_content, config.traffic_compensation_cycle_number, config.phase_weight);
        break;
    case 3:
        traffic_compensation_method3(log_content, config.traffic_compensation_cycle_number);
        break;
    default:
        break;
    }
    report_compensation_time();
NotReportEnd:
    // 補償結束清空 compensation buffer
    compensation_buffer_clear();
    log_file_write(log_content);
}