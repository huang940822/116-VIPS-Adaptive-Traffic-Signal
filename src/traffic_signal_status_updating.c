#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <errno.h>

#include "log.h"
#include "config.h"
#include "typedefine.h"
#include "error_status.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "traffic_signal_packet_tx.h"

traffic_signal_status_t signal_status;
pthread_mutex_t mutex_signal_status = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_signal_status;
static uint8_t pretime_sent_count=0;
extern uint8_t flag_pretime;

/* 5F CC 回報時相步階 */
void packet_5FCC(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);
    
    signal_status.ControlStrategy = packet->INFO[2];
    //here is to map 成龍 controlstrategy to be same as 山住;0/1 成龍/三住
    if(config.signal_controller_manufacturer == 0 && signal_status.ControlStrategy == 21){
        log_file_write("map cheng_long dynamic controlstrategy from 21 to 16\n\r");
        signal_status.ControlStrategy=16;
    }else if(config.signal_controller_manufacturer == 0 && signal_status.ControlStrategy == 5){
        log_file_write("map cheng_long pretime controlstrategy from 5 to 1\n\r");
        signal_status.ControlStrategy=1;
    }else{
        log_file_write("no control strategy map action should be taken\n\r");
    }
    
    
    // for some error situation happens in CHENG_LONG
    if (packet->INFO[3] != 0 && packet->INFO[4] != 5) {
        signal_status.SubPhaseID = packet->INFO[3];
    }
    signal_status.StepID = packet->INFO[4];
    signal_status.StepSec = (packet->INFO[5] << 8) | packet->INFO[6];


    // execute pretime instruction to force tc go back to pretime
    // to prevent the tc not go back to pretime after 全動態
    // pretime_sent_count is for let pretime sent one time only in step 4
    if(signal_status.StepID == 4){    //4 閃黃燈
        if(pretime_sent_count == 0){
            log_file_write("5FCC: execute go back to pretime at step 4\r\n");
            printf("5FCC updating: set pretime!\r\n");
            //tsc_pretime();
            flag_pretime=true;
        }
        pretime_sent_count++;
    }else{
        pretime_sent_count=0;
    }


    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "signal packet info: 5FCC");
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nControlStrategy: %d", signal_status.ControlStrategy);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseID: %d", signal_status.SubPhaseID);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nStepID: %d", signal_status.StepID);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nStepSec: %d", signal_status.StepSec);
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);

    int sem_value;
    //sem初始值為1 所以0代表只有自己近來的狀況嗎？
    sem_getvalue(&sem_signal_status, &sem_value);
    if (sem_value == 0) {
        sem_post(&sem_signal_status);
    }

    return;
}

/* 5F C8 回報當前時制計畫編號 */
void packet_5FC8(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    pthread_mutex_lock(&mutex_signal_status);
    signal_status.PlanID = packet->INFO[2];
    
    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "signal packet info: 5FC8");
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nPlanID: %d", signal_status.PlanID);
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

/* 5F C5 回報時制計畫編號與資料庫 */    //收tc箱資料
void packet_5FC5(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    pthread_mutex_lock(&mutex_signal_status);
    // signal_status.PlanID = packet->INFO[2];
    if (signal_status.PhaseOrder != packet->INFO[4]) {
        command_buf_clear();
    }
    signal_status.PhaseOrder = packet->INFO[4];
    signal_status.SubPhaseCount = packet->INFO[5];
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        signal_status.plan[i].Green = (packet->INFO[6 + i * 2] << 8 | packet->INFO[7 + i * 2]);
    }
    signal_status.CycleTime = packet->INFO[6 + signal_status.SubPhaseCount * 2] << 8 | packet->INFO[7 + signal_status.SubPhaseCount * 2];
    signal_status.Offset = packet->INFO[8 + signal_status.SubPhaseCount * 2] << 8 | packet->INFO[9 + signal_status.SubPhaseCount * 2];
    
    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "signal packet info: 5FC5");
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nPhaseOrder: %d", signal_status.PhaseOrder);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseCount: %d", signal_status.SubPhaseCount);
        for (int i = 0; i < signal_status.SubPhaseCount; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nGreen: %d", signal_status.plan[i].Green);
        }
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nCycleTime: %d", signal_status.CycleTime);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nOffset: %d", signal_status.Offset);
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

/* 5F C4 回報時制計畫基本參數 */
void packet_5FC4(traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_signal_status);
    signal_status.SubPhaseCount = packet->INFO[3];
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        signal_status.plan[i].MinGreen = packet->INFO[4 + i * 7];
        signal_status.plan[i].MaxGreen = (packet->INFO[5 + i * 7] << 8 | packet->INFO[6 + i * 7]);
        signal_status.plan[i].Yellow = packet->INFO[7 + i * 7];
        signal_status.plan[i].AllRed = packet->INFO[8 + i * 7];
        signal_status.plan[i].PedGreenFlash = packet->INFO[9 + i * 7];
        signal_status.plan[i].PedRed = packet->INFO[10 + i * 7];

        signal_status.plan[i].PreGreen = signal_status.plan[i].Green - signal_status.plan[i].PedGreenFlash;
    }
    
    if (config.log_signal_packet_info) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "signal packet info: 5FC4");
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseCount: %d", signal_status.SubPhaseCount);
        for (int i = 0; i < signal_status.SubPhaseCount; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nstatus.plan[%d] G:%2d minG:%2d maxG:%3d Y:%1d AR:%1d PG:%1d PR:%1d PreG:%2d", 
                i, signal_status.plan[i].Green, signal_status.plan[i].MinGreen, signal_status.plan[i].MaxGreen, signal_status.plan[i].Yellow, 
                signal_status.plan[i].AllRed, signal_status.plan[i].PedGreenFlash, signal_status.plan[i].PedRed, signal_status.plan[i].PreGreen);
        }
        log_file_write(log_content);
    }
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

void packet_5F0C(traffic_signal_packet_t *packet)
{
    
}


void packet_0F04(traffic_signal_packet_t *packet)
{   
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    // printf("tc status info: ");
    
    uint16_t original_tc_hstatus=packet->INFO[2]<<8|packet->INFO[3];
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),"original_tc_health_status is %04X\n\r",original_tc_hstatus);
    log_file_write(log_content);
    //dont show bit 14, 8, 9 for they seprately means controller ready, cabinated opened, communication connect
    original_tc_hstatus=original_tc_hstatus&0xbcff; 
    printf("tc status\n\r");
    printf("%04X\n\r", original_tc_hstatus);
    if(original_tc_hstatus != 0){
        set_tsc_error();
    }else{
        clear_tsc_error();
    }

}

//裡面有些部份看不太懂 為何要用號誌加上mutex保護
void get_traffic_signal_status(traffic_signal_status_t *traffic_signal_status)
{   //wait what??
    sem_timedwait_millsecs(&sem_signal_status, SEM_SIGNAL_STATUS_TIMEOUT);

    pthread_mutex_lock(&mutex_signal_status);
    //signal_status global variable and it's why use mutex to protect
    memcpy(traffic_signal_status, &signal_status, sizeof(traffic_signal_status_t));
    pthread_mutex_unlock(&mutex_signal_status);

    //號誌的釋放
    int sem_value;
    sem_getvalue(&sem_signal_status, &sem_value);
    if (sem_value == 0) {
        sem_post(&sem_signal_status);
    }

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

//不同的step進來看到的remaining time不一樣 用自己剩餘的秒數 在加上還沒跑得step的秒數
//就是remaining time
uint16_t get_remaining_time(uint8_t phase, uint8_t step, uint16_t second)
{
    pthread_mutex_lock(&mutex_signal_status);
    switch (step)
    {
    case 1:
        second = second + signal_status.plan[phase - 1].PedGreenFlash 
                        + signal_status.plan[phase - 1].Yellow 
                        + signal_status.plan[phase - 1].PedRed
                        + signal_status.plan[phase - 1].AllRed;
        break;
    case 2:
        second = second + signal_status.plan[phase - 1].Yellow 
                        + signal_status.plan[phase - 1].PedRed
                        + signal_status.plan[phase - 1].AllRed;
        break;
    case 3:
        second = second + signal_status.plan[phase - 1].PedRed
                        + signal_status.plan[phase - 1].AllRed;
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

void set_control_status(uint8_t control_status)
{
    pthread_mutex_lock(&mutex_signal_status);
    signal_status.control_status = control_status;
    pthread_mutex_unlock(&mutex_signal_status);
    return;
}

//這個函式在幹麻？？ 要廣播給obu現在tc箱的狀況
void report_plan()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *)malloc(R2V_SPECIFIC_FIELD_MAX_LEN);
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
    write_uint8_t(signal_status.PlanID,&write_buf);
    // cycle
    write_uint16_t(signal_status.CycleTime, &write_buf);
    // offset
    write_uint16_t(signal_status.Offset, &write_buf);
    // subphase count
    write_uint8_t(signal_status.SubPhaseCount, &write_buf);
    // static plan
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        write_uint8_t(i + 1, &write_buf); // subphase id
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
** Description: semaphore wait a limit on the amount of time specified in milliseconds.
** Parameter:   sem: semaphore handle
**              msecs: timeout value
** Return:      none
** Reference:   https://blog.csdn.net/wy5761/article/details/9320331
******************************************************************************/
void sem_timedwait_millsecs(sem_t *sem, long msecs)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long secs = msecs/1000;
	msecs = msecs%1000;
	
	long add = 0;
	msecs = msecs*1000*1000 + ts.tv_nsec;
	add = msecs / (1000*1000*1000);
	ts.tv_sec += (add + secs);
	ts.tv_nsec = msecs%(1000*1000*1000);
 
	if (sem_timedwait(sem, &ts) == -1) {
        log_file_write_fatal_error("sem_timedwait_millsecs: sem_timedwait (%d)", errno);
        perror("sem_timedwait_millsecs: sem_timedwait");
    } else {
        return;
    }
}