#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "OBU_record_processing.h"
#include "application_registration.h"
#include "buffer.h"
#include "com_packet_processing.h"
#include "config.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

// extern pthread_mutex_t mutex_rs232_write;
extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;
extern uint8_t flag_query_firm_ver;
extern uint8_t flag_switch2nextStep;
extern uint8_t flag_PhaseOrder;
extern pthread_mutex_t mutex_uart_comple_protect;
extern buffer_ring_t *DSRC_send_buffer;
static unsigned int count = 0;
struct tm *localTime;
void timer_event_handler(__sigval_t value)
{
    if (*(uint8_t *) value.sival_ptr ==
        TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT) {
        if (config.log_middleware_timer_event) {
            log_file_write("timer event: traffic signal status report");
        }
        // report_plan();  //對obu 廣播 plan
    } else if (*(uint8_t *) value.sival_ptr ==
               TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING) {
        if (config.log_middleware_timer_event) {
            log_file_write("timer event: traffic signal command buf polling");
        }

        pthread_mutex_lock(&mutex_uart_comple_protect);
        // printf("get in uart mutex\r\n");
        // pthread_mutex_lock(&mutex_rs232_write);
        static uint8_t flag_query_allday_plan = true;
        uint8_t temp_ack_seq;

        temp_ack_seq = tsc_5F48();  //查詢目前時制計劃內容
        // WAIT_ACK_LOOP
        temp_ack_seq = tsc_5F4C();  //查詢號控器目前時相及步階
        // WAIT_ACK_LOOP
        temp_ack_seq = tsc_5F45();  //查詢時制計劃之設定內容
        // WAIT_ACK_LOOP
        temp_ack_seq = tsc_5F44();
        // WAIT_ACK_LOOP
        command_buf_polling();

        if (count == 0) {
            temp_ack_seq = tsc_0F42();  //查詢日期、時間
            WAIT_ACK_LOOP
            count++;
        } else {
            count++;
            if (count == 7200)
                count = 0;
        }
        if(flag_PhaseOrder == true){
            temp_ack_seq = tsc_5F43();
            WAIT_ACK_LOOP
            flag_PhaseOrder = false;
        }
        if (flag_pretime == true) {
            temp_ack_seq = tsc_pretime();
            WAIT_ACK_LOOP
            flag_pretime = false;
        }

        if (flag_countdown_on == true) {
            temp_ack_seq =
                tsc_countdown_on(config.signal_controller_manufacturer);
            WAIT_ACK_LOOP
            flag_countdown_on = false;
        }

        if (flag_countdown_off == true) {
            temp_ack_seq =
                tsc_countdown_off(config.signal_controller_manufacturer);
            WAIT_ACK_LOOP
            flag_countdown_off = false;
        }
        if (flag_query_firm_ver == true) {
            temp_ack_seq = tsc_query_firmware_version();
            WAIT_ACK_LOOP
            flag_query_firm_ver = false;
        }
        //當發生655xx秒數時 強制切到下一個step
        if (flag_switch2nextStep == true) {
            // traffic_signal_status_t signal_status;
            // get_traffic_signal_status(&signal_status);
            temp_ack_seq = tsc_dynamic();
            WAIT_ACK_LOOP
            //下0 讓step1立刻結束
            temp_ack_seq = tsc_switch();
            WAIT_ACK_LOOP
            flag_switch2nextStep = false;
            log_file_write(
                "step sec higher than 255 happens and switch to next step "
                "forcelly!!\r\n");
        }
        if (flag_query_allday_plan == true) {
            time_t currentTime;
            time(&currentTime);
            localTime = localtime(&currentTime);
            temp_ack_seq = tsc_5F46(localTime->tm_wday);
            WAIT_ACK_LOOP
            flag_query_allday_plan = false;
        }

        pthread_mutex_unlock(&mutex_uart_comple_protect);
        // printf("leave uart write mutex\r\n");

        // pthread_mutex_unlock(&mutex_rs232_write);
    } else if (*(uint8_t *) value.sival_ptr ==
               TIMER_EVENT_LOG_FILE_NAME_UPDATE) {
        if (config.log_middleware_timer_event) {
            log_file_write("timer event: log file name update");
        }

        log_file_name_update();
    } else if (*(uint8_t *) value.sival_ptr ==
               TIMER_EVENT_ENFORCE_PRETIME) {
        time_t currentTime;
        time(&currentTime);
        localTime = localtime(&currentTime);
        int hour = localTime->tm_hour;        
        int minute = localTime->tm_min;       
        int second = localTime->tm_sec;

        traffic_signal_status_t signal_status;
        get_traffic_signal_status(&signal_status);
        uint16_t cycle_time = signal_status.CycleTime;

        for (int i = 0; i < signal_status.SegmentCount; i++) {
            uint8_t planHour = signal_status.allday_plan[i].Hour;
            uint8_t planMin = signal_status.allday_plan[i].Min;
            // 80 110 140
            // 6:50 + 160s 
            int timeDiff = (planHour - localTime->tm_hour) * 3600 + (planMin - localTime->tm_min) * 60;
            // printf("Approaching PlanID %d - Time Remaining: %d seconds\n", signal_status.allday_plan[i].PlanID, timeDiff);
            // config
            if (timeDiff >= -600 && timeDiff <= 30) {
                tsc_pretime();
                command_buf_clear();
                log_file_write("Enforce to pretime control_strategy\r\n");
                log_file_write("command buffer clear\r\n");
            }
        }
    }
    // 在thread pool 中傳 bsm 的 timer
    // else if (*(uint8_t *) value.sival_ptr == TIMER_EVENT_DSRC_SEND) {
    // if (config.log_middleware_timer_event) {
    //     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
    //     strlen(log_content), "%s", "timer event: OBU list garbage
    //     collection"); log_file_write(log_content);
    // }
    // DSRC_send_timer_handler(DSRC_send_buffer);
    // }
}

/*****************************************************************************
** Function:    create_timer
** Description: Create a timer.
** Parameter:   timer_id: timer_id of timer
**              signal_value: data passed with notification
**              notify_function: notify_function is called when timer expires.
** Return:      0: create timer successfully
** Reference:   https://blog.csdn.net/p942005405/article/details/100883568
******************************************************************************/

int create_timer(timer_t *timer_id,
                 void *signal_value,
                 void (*notify_function)(__sigval_t))
{
    struct sigevent evp;
    memset(&evp, 0, sizeof(struct sigevent));

    /* Set and enable alarm */
    /* SIGEV_NONE：什麼都不做,只提供通過 timer_gettime 和 timer_getoverrun
     * 查詢超時訊息 */
    /* SIGEV_SIGNAL: 當定時器到期,內核會將 sigev_signo 所指定的信號傳給進程
     * 在信號處理程序中 si_value 會被設定為 sigev_value */
    /* SIGEV_THREAD: 當定時器到期,內核會(在此進程內)以
     * sigev_notification_attributes 為線程屬性創建一個線程,並且讓它執行
     * sigev_notify_function,並傳入 sigev_value 作為參數 */

    evp.sigev_value.sival_ptr =
        signal_value;                             //用於標識定時器
                                                  //(這和timerid有什麼區別？回調函數可以獲得)
    evp.sigev_notify = SIGEV_THREAD;              //線程通知的方式，派駐新線程
    evp.sigev_notify_function = notify_function;  //線程函數地址
    if (timer_create(CLOCK_REALTIME, &evp, timer_id) == -1) {
        log_file_write_fatal_error("create_timer: timer_create");
        perror("create_timer: timer_create");
        exit(errno);
    } else {
        if (signal_value != NULL && config.log_middleware_timer_event == 1) {
            log_file_write("timer event: create timer with signal value(%d)",  *(uint8_t *) signal_value);
        }
        return 0;
    }
}

/*****************************************************************************
** Function:    set_timer
** Description: Set timer interval & initial expiration.
** Parameter:   timer_id: timer_id of timer
**              interval_sec: timer interval specified in seconds
**              interval_nsec: timer interval specified in nanoseconds
**              initial_sec: initial expiration specified in seconds
**              initial_nsec: initial expiration specified in nanoseconds
** Return:      0: set timer successfully
******************************************************************************/
int set_timer(timer_t timer_id,
              int32_t interval_sec,
              int32_t interval_nsec,
              uint32_t initial_sec,
              uint32_t initial_nsec)
{
    struct itimerspec its;
    /* timer interval */
    its.it_interval.tv_sec = interval_sec;
    its.it_interval.tv_nsec = interval_nsec;
    /* initial expiration */
    its.it_value.tv_sec = initial_sec;
    its.it_value.tv_nsec = initial_nsec;
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {
        log_file_write_fatal_error("set_timer: timer_settime");
        perror("set_timer: timer_settime");
        exit(errno);
    } else {
        return 0;
    }
}

int delete_timer(timer_t timer_id)
{
    if (timer_delete(timer_id) == -1) {
        log_file_write_fatal_error("delete_timer: timer_delete");
        perror("delete_timer: timer_delete");
        exit(errno);
    } else {
        return 0;
    }
}