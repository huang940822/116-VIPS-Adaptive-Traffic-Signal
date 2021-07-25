#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "log.h"
#include "config.h"
#include "typedefine.h"
#include "timer_event.h"
#include "OBU_record_processing.h"
#include "application_registration.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"

// extern pthread_mutex_t mutex_rs232_write;
extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;
extern uint8_t flag_query_firm_ver;
// extern int16_t ack_seq;

void timer_event_handler(__sigval_t value)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    if (*(uint8_t *)value.sival_ptr == TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT) {
        if (config.log_middleware_timer_event) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "timer event: traffic signal status report");
            log_file_write(log_content);
        }
        
        report_plan();
    }
    else if (*(uint8_t *)value.sival_ptr == TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING) {
        if (config.log_middleware_timer_event) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "timer event: traffic signal command buf polling");
            log_file_write(log_content);
        }
        
        command_buf_polling();
        // pthread_mutex_lock(&mutex_rs232_write);
        uint8_t temp_ack_seq;
        temp_ack_seq=tsc_5F4C();
        WAIT_ACK_LOOP
        temp_ack_seq=tsc_5F48();
        WAIT_ACK_LOOP
        temp_ack_seq=tsc_5F45();
        WAIT_ACK_LOOP
        temp_ack_seq=tsc_5F44();
        WAIT_ACK_LOOP
        if(flag_pretime==true){
            temp_ack_seq=tsc_pretime();
            WAIT_ACK_LOOP
            flag_pretime=false;
        }
        
        if(flag_countdown_on==true){
            temp_ack_seq=tsc_countdown_on(config.signal_controller_manufacturer);
            WAIT_ACK_LOOP
            flag_countdown_on=false;
        }
        
        if(flag_countdown_off==true){
            temp_ack_seq=tsc_countdown_off(config.signal_controller_manufacturer);
            WAIT_ACK_LOOP
            flag_countdown_off=false;
        }
        if(flag_query_firm_ver==true){
            temp_ack_seq=tsc_query_firmware_version();
            WAIT_ACK_LOOP
            flag_query_firm_ver=false;

        }

        // pthread_mutex_unlock(&mutex_rs232_write);
    }
    else if (*(uint8_t *)value.sival_ptr == TIMER_EVENT_OBU_LIST_GARBAGE_COLLECTION) {
        if (config.log_middleware_timer_event) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "timer event: OBU list garbage collection");
            log_file_write(log_content);
        }

        OBU_object_garbage_collection();
        OBU_object_print();
    }
    else if (*(uint8_t *)value.sival_ptr == TIMER_EVENT_LOG_FILE_NAME_UPDATE) {
        if (config.log_middleware_timer_event) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "timer event: log file name update");
            log_file_write(log_content);
        }
        
        log_file_name_update();
    }
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

int create_timer(timer_t *timer_id, void *signal_value, void (*notify_function)(__sigval_t)) 
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    struct sigevent evp;
    memset(&evp, 0, sizeof(struct sigevent));

    /* Set and enable alarm */ 
    /* SIGEV_NONE：什麼都不做,只提供通過 timer_gettime 和 timer_getoverrun 查詢超時訊息 */
    /* SIGEV_SIGNAL: 當定時器到期,內核會將 sigev_signo 所指定的信號傳給進程 在信號處理程序中 si_value 會被設定為 sigev_value */
    /* SIGEV_THREAD: 當定時器到期,內核會(在此進程內)以 sigev_notification_attributes 為線程屬性創建一個線程,並且讓它執行 sigev_notify_function,並傳入 sigev_value 作為參數 */

    evp.sigev_value.sival_ptr = signal_value;		    //用於標識定時器 (這和timerid有什麼區別？回調函數可以獲得)
	evp.sigev_notify = SIGEV_THREAD;		            //線程通知的方式，派駐新線程
	evp.sigev_notify_function = notify_function;	//線程函數地址 
    if (timer_create(CLOCK_REALTIME, &evp, timer_id) == -1) {
        log_file_write_fatal_error("create_timer: timer_create");
        perror("create_timer: timer_create");
        exit(errno);
    } else {
        if (signal_value != NULL && config.log_middleware_timer_event == 1) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "timer event: ");
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "create timer with signal value(%d)", *(uint8_t *)signal_value);
            log_file_write(log_content);
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
int set_timer(timer_t timer_id, int32_t interval_sec, int32_t interval_nsec, uint32_t initial_sec, uint32_t initial_nsec) 
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