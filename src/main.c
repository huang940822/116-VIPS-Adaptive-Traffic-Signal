#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "log.h"
#include "TSP.h"
#include "EVSP.h"
#include "server.h"
#include "config.h"
#include "EVSP_config.h"
#include "msg_queue.h"
#include "dispatcher.h"
#include "typedefine.h"
#include "timer_event.h"
#include "byte_processing.h"
#include "OBU_record_processing.h"
#include "application_registration.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "error_status.h"
extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;
extern uint8_t flag_query_firm_ver;

int main()
{   
    //below is for tsc_countdown_on/off test
    // tsc_countdown_off(0);
    // sleep(10);
    // tsc_countdown_on(0);
    // return 0;

    /* Start server */
    int ret = 0;
    char log_content[LOG_CONTENT_LEN + 1];

    /* log init */
    log_file_init();//一個timer被created

    //init dsrc error detect
    dsrc_error_detect_init();

    /* read config file*/
    ret = config_init();
    if (ret != 0 ) {
        log_file_write_fatal_error("error reading config file: %d", ret);
    }

    /* read evsp confile file*/
    ret = EVSP_config_init();
    if (ret != 0 ) {
        log_file_write_fatal_error("error evsp reading config file: %d", ret);
    }

    printf("query tc firmware version\r\n");
    flag_query_firm_ver=true;

    /* application service registration */
    // EVSP
    ret = app_register(&EVSP);
    if (ret != 0 ) {
        log_file_write_fatal_error("error registering application: %d (%s)", ret, "EVSP");
    } else {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s register successfully", EVSP.name);
        log_file_write(log_content);
    }
    // TSP
    ret = app_register(&TSP);
    if (ret != 0 ) {
        log_file_write_fatal_error("error registering application: %d (%s)", ret, "TSP");
    } else {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s register successfully", TSP.name);
        log_file_write(log_content);
    }
    // log application register event
    if (config.log_application_register_event) {
        event_callback_print();
    }

    /* taffic signal packet serial port init */
    traffic_signal_port_init();

    /* Receive traffic signal packet */
	pthread_t traffic_signal_packet_rx_thread;
    ret = pthread_create(&traffic_signal_packet_rx_thread, NULL, traffic_signal_packet_rx_handler, NULL);
	if (ret != 0) {
        log_file_write_fatal_error("error creating traffic_signal_packet_rx_thread: %d", ret);
		perror("main: pthread_create");
        exit(errno);
	}

    /* command buffer init & command buffer polling timer event*/
    command_buf_init(); //這裡面又一個timer被created

    /* traffic signal status report timer event */  //這裡是幹麻看不懂 r2v???
    if (config.signal_status_report_active) {
        timer_t traffic_signal_status_report_timer_id;
        uint8_t traffic_signal_status_report_timer_num = TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT;

        create_timer(&traffic_signal_status_report_timer_id, &traffic_signal_status_report_timer_num, timer_event_handler);
        set_timer(traffic_signal_status_report_timer_id, 1, 0, 1, 0);
    }
    
    /* OBU list garbage collection timer event */   //清掉太久的obu object
    timer_t OBU_list_garbage_collection_timer_id;
    uint8_t OBU_list_garbage_collection_timer_num = TIMER_EVENT_OBU_LIST_GARBAGE_COLLECTION;
    create_timer(&OBU_list_garbage_collection_timer_id, &OBU_list_garbage_collection_timer_num, timer_event_handler);
    set_timer(OBU_list_garbage_collection_timer_id, 1, 0, 1, 0);

    /* Packet dispatcher */
	pthread_t dispatcher_thread;
    ret = pthread_create(&dispatcher_thread, NULL, dispatcher_handler, NULL);
	if (ret != 0) {
        log_file_write_fatal_error("error creating dispatcher_thread: %d", ret);
		perror("main: pthread_create");
        exit(errno);
	}
    
	com_layer_init(NULL);

    // int input, temp_ack_seq;
    // while(1){
    //     printf("input function number:\r\n0:countdownoff\r\n1:countdownon\r\n2:effecttime 200\r\n3:pretime\r\n4:send 5f4c\r\n5:query tc firmware version\r\n");
    //     scanf("%d",&input);

    //     switch (input)
    //     {
    //     case 0:
    //         log_file_write("countdown off\r\n");
    //         printf("count down off\r\n");
    //         flag_countdown_off=1;
    //         break;
    //     case 1:
    //         log_file_write("countdown on\r\n");
    //         printf("count down on");
    //         flag_countdown_on=1;
    //         break;
    //     case 2:
    //         log_file_write("effect time 200\r\n");
    //         printf("set effect time 200\r\n");
    //         temp_ack_seq=tsc_dynamic();
    //         WAIT_ACK_LOOP
    //         temp_ack_seq=tsc_extend(1,1,200);
    //         WAIT_ACK_LOOP
    //         break;
    //     case 3:
    //         log_file_write("go to pretime");
    //         printf("go to pretime\r\n");
    //         flag_pretime=1;
    //         break;
    //     case 4:
    //         printf("do nothing\r\n");
    //         // printf("send 5f4c\r\n");
    //         // temp_ack_seq=tsc_5F4C();
    //         // WAIT_ACK_LOOP
    //         break;
    //     case 5:
    //         printf("query tc firmware version\r\n");
    //         flag_query_firm_ver=true;
    //         break;
    //     default:
    //         break;
    //     }
    // }

    pthread_exit(0);
    
    return 0;
}