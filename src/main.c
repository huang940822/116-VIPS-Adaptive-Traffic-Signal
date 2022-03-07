#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CPS.h"
#include "EVSP.h"
#include "EVSP_config.h"
#include "OBU_record_processing.h"
#include "TSP.h"
#include "TSP_config.h"
#include "application_registration.h"
#include "byte_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "error_code_user.h"
#include "error_status.h"
#include "j2735_codec.h"
#include "log.h"
#include "msg_queue.h"
#include "server.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;
extern uint8_t flag_query_firm_ver;

pthread_mutex_t mutex_uart_comple_protect = PTHREAD_MUTEX_INITIALIZER;


void sigintHandler(int sig_num)
{
    signal(SIGINT, sigintHandler);
    pthread_mutex_lock(&mutex_uart_comple_protect);
    printf("get in mutex in signal handler\r\n");
    printf(
        "\nuart write actions has all be completed before exit from process\n");
    fflush(stdout);
    exit(0);
    pthread_mutex_unlock(&mutex_uart_comple_protect);
}


int main()
{
    signal(SIGINT, sigintHandler);

    /* Start server */
    int ret = 0;
    char log_content[LOG_CONTENT_LEN + 1];

    /* log init */
    log_file_init();  //一個timer被created

    // init dsrc error detect
    dsrc_error_detect_init();
    // init tc fail detect
    tc_5fcc_error_detect_init();

    /* read config file*/
    ret = config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error reading config file: %d", ret);
    }

    /* read evsp confile file*/
    ret = EVSP_config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error evsp reading config file: %d", ret);
    }

    /* read tsp confile file*/
    ret = TSP_config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error tsp reading config file: %d", ret);
    }

    /* read spat confile file*/
    ret = SPaT_config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error spat reading config file: %d", ret);
    }

    /* read map confile file*/
    // ret = MAP_config_init();
    // if(ret != 0) {
    //     log_file_write_fatal_error("error map reading config file: %d", ret);
    // }

    printf("query tc firmware version\r\n");
    flag_query_firm_ver = true;

    /* application service registration */
    // EVSP
    ret = app_register(&EVSP);
    if (ret != 0) {
        log_file_write_fatal_error("error registering application: %d (%s)",
                                   ret, "EVSP");
    } else {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s register successfully", EVSP.name);
        log_file_write(log_content);
    }
    // TSP
    ret = app_register(&TSP);
    if (ret != 0) {
        log_file_write_fatal_error("error registering application: %d (%s)",
                                   ret, "TSP");
    } else {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s register successfully", TSP.name);
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
    ret = pthread_create(&traffic_signal_packet_rx_thread, NULL,
                         traffic_signal_packet_rx_handler, NULL);
    if (ret != 0) {
        log_file_write_fatal_error(
            "error creating traffic_signal_packet_rx_thread: %d", ret);
        perror("main: pthread_create");
        exit(errno);
    }

    /* command buffer init & command buffer polling timer event*/
    command_buf_init();  //這裡面又一個timer被created

    /* traffic signal status report timer event */  //這裡是幹麻看不懂 r2v???
    if (config.signal_status_report_active) {
        timer_t traffic_signal_status_report_timer_id;
        uint8_t traffic_signal_status_report_timer_num =
            TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT;

        create_timer(&traffic_signal_status_report_timer_id,
                     &traffic_signal_status_report_timer_num,
                     timer_event_handler);
        set_timer(traffic_signal_status_report_timer_id, 1, 0, 1, 0);
    }

    /* OBU list garbage collection timer event */  //清掉太久的obu object

    timer_t OBU_list_garbage_collection_timer_id;
    uint8_t OBU_list_garbage_collection_timer_num =
        TIMER_EVENT_OBU_LIST_GARBAGE_COLLECTION;
    create_timer(&OBU_list_garbage_collection_timer_id,
                 &OBU_list_garbage_collection_timer_num, timer_event_handler);
    set_timer(OBU_list_garbage_collection_timer_id, 1, 0, 1, 0);

    // CPS
    ret = app_register(&CPS);

    if (ret != 0) {
        log_file_write_fatal_error("error registering application: %d (%s)",
                                   ret, "CPS");
    } else {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s register successfully", CPS.name);
        log_file_write(log_content);
    }

    // SPaT
    // ret = app_register(&SPaT);
    // if(ret != 0) {
    //    log_file_write_fatal_error("error registering application: %d (%s)",
    //    ret, "SPaT");
    // } else {
    //     memset(log_content, 0, sizeof(log_content));
    //     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
    //     strlen(log_content), "%s register successfully", SPaT.name);
    //     log_file_write(log_content);
    // }
    // // MAP
    // ret = app_register(&MAP);
    // if(ret != 0) {
    //     log_file_write_fatal_error("error registering application: %d (%s)",
    //     ret, "MAP");
    // } else {
    //     memset(log_content, 0, sizeof(log_content));
    //     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
    //     strlen(log_content), "%s register successfully", MAP.name);
    //     log_file_write(log_content);
    // }

    event_callback_print();

    /*DSRC send timer event*/
    // timer_t DSRC_send_timer_id;
    // uint8_t DSRC_send_timer_id_num = TIMER_EVENT_DSRC_SEND;
    // create_timer(&DSRC_send_timer_id, &DSRC_send_timer_id_num,
    // timer_event_handler); set_timer(DSRC_send_timer_id, 0, 10000, 1, 0);

    /* Packet dispatcher */
    pthread_t dispatcher_thread;
    ret = pthread_create(&dispatcher_thread, NULL, dispatcher_handler, NULL);
    if (ret != 0) {
        log_file_write_fatal_error("error creating dispatcher_thread: %d", ret);
        perror("main: pthread_create");
        exit(errno);
    }
    J2735Config cfg;
    ret = j2735_init(&cfg);
    if (!IS_SUCCESS(ret)) {
        printf("Fail to init J2735\n");
        return -1;
    }
    /* Start server */

    com_layer_init(NULL);

    // int input, temp_ack_seq;
    int c, d, a, b;
    int a1, b1, c1, d1;
    traffic_signal_status_t signal_status;

    // // usleep(10000000);
    // // get_traffic_signal_status(&signal_status);

    // // printf("pretime for phase 2 is %d phase 4 is %d\r\n", c,d);
    // tsc_command_t test_a, test_b;
    // uint8_t flag=true;
    // // uint8_t flag_2=false;
    // uint8_t current_phase=signal_status.SubPhaseID;
    // // flag_countdown_off=true;
    // printf("input the sec want to adjust for phase:\r\n");
    // scanf("%d",&input);


    // SPAT *p_spat;
    // uint8_t *tx_buf = NULL;
    // int tx_buf_len = 0;
    // if(Spat_msg_init(&p_spat) == 0) {
    //    printf("error on init\n");
    //    return 0;
    //}
    // traffic_signal_status_t qsignal_status;

    while (1) {
        // get_traffic_signal_status(&qsignal_status);
        // int phase = get_current_phase()-1;
        // printf("count = %d\n\n", qsignal_status.SubPhaseCount);
        // printf("\nphase = %d\n", phase);
        // printf("GREEN :%d, pedgreen :%d, yellow :%d, red
        // :%d\n",qsignal_status.plan[0].Green,qsignal_status.plan[0].PedGreenFlash,qsignal_status.plan[0].Yellow,qsignal_status.plan[0].AllRed);
        // printf("GREEN :%d, pedgreen :%d, yellow :%d, red
        // :%d\n",qsignal_status.plan[1].Green,qsignal_status.plan[1].PedGreenFlash,qsignal_status.plan[1].Yellow,qsignal_status.plan[1].AllRed);
        // printf("GREEN :%d, pedgreen :%d, yellow :%d, red
        // :%d\n",qsignal_status.plan[2].Green,qsignal_status.plan[2].PedGreenFlash,qsignal_status.plan[2].Yellow,qsignal_status.plan[2].AllRed);
        // printf("GREEN :%d, pedgreen :%d, yellow :%d, red
        // :%d\n",qsignal_status.plan[3].Green,qsignal_status.plan[3].PedGreenFlash,qsignal_status.plan[3].Yellow,qsignal_status.plan[3].AllRed);
        // printf("now second :%d\n",get_current_second());
        // spat_msg_update(&p_spat);
        // tx_buf_len = Compose_spat(&tx_buf, p_spat);
        // printf("SPAT encoded data:\n");
        // dump_mem(tx_buf, tx_buf_len);
        // decode_spat(tx_buf, tx_buf_len);
    }
    //     // printf("count down off\r\n");
    //     // flag_countdown_off=1;
    while (1) {
        //     // printf("count down off\r\n");
        //     // flag_countdown_off=1;

        //     // temp_ack_seq=tsc_dynamic();
        //     // WAIT_ACK_LOOP
        //     // //不能下0 否則step會立刻結束
        //     // temp_ack_seq=tsc_extend(1, 1, 51);//每次就是pretime-4去扣
        //     // WAIT_ACK_LOOP

        //     // break;
        //     // printf("input the sec want to adjust for phase 1:\r\n");
        //     // scanf("%d",&input);

        get_traffic_signal_status(&signal_status);

        //     current_phase=signal_status.SubPhaseID;
        //     // printf("current phase is %d\r\n", current_phase);
        a = signal_status.plan[0].PreTimeCompensated;
        b = signal_status.plan[1].PreTimeCompensated;
        c = signal_status.plan[2].PreTimeCompensated;
        d = signal_status.plan[3].PreTimeCompensated;
        a1 = signal_status.plan[0].PreGreen;
        b1 = signal_status.plan[1].PreGreen;
        c1 = signal_status.plan[2].PreGreen;
        d1 = signal_status.plan[3].PreGreen;

        char log_content[LOG_CONTENT_LEN + 1];
        // memset(log_content, 0, sizeof(log_content));
        // if (config.log_command_buffer) {
        // snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
        // strlen(log_content), "command_buf_send: ");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "pretime_compensated for phase 1 is %d phase 2 is %d phase 3 "
                 "is %d phase 4 is %d\r\n",
                 a, b, c, d);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "pretime             for phase 1 is %d phase 2 is %d phase 3 "
                 "is %d phase 4 is %d\r\n",
                 a1, b1, c1, d1);
        log_file_write(log_content);

        printf("StepSec: %d\r\n", get_current_second());

        // if(flag){
        //     printf("flag:%d\r\n",flag);
        //     get_compensation_buffer(compensation_time);
        //     flag = false;
        // }

        // }

        // printf("pretime_compensated for phase 1 is %d phase 2 is %d phase 3
        // is %d phase 4 is %d\r\n", a, b, c,d); printf("pretime             for
        // phase 1 is %d phase 2 is %d phase 3 is %d phase 4 is %d\r\n", a1, b1,
        // c1,d1);
        //     // printf("flag_1 is %d\r\n", flag_1);
        //     if(current_phase==2){
        //         if(flag==true){
        //             test_a.adjustment=input;
        //             // flag=false;
        //             // printf("adjust %d\r\n", test_b.adjustment);
        //         }
        //         // else{
        //         // // test_a.adjustment=-(c-4);
        //         //     test_a.adjustment=-4;
        //         //     printf("adjust %d\r\n", test_b.adjustment);
        //         //     // test_a.adjustment=5;
        //         //     flag=true;
        //         // }
        //         // flag_1=false;
        //         // flag_2=true;

        //         test_a.app_id=TSP.id;
        //         test_a.app_priority=TSP.priority;
        //         test_a.cycle=0;
        //         memcpy(test_a.host_OBU_id,"bus_168",7);
        //         // printf("host id is %s\r\n", test_a.host_OBU_id);
        //         test_a.phase=2;
        //         test_a.target_phase=1;
        //         test_a.effect_time=1;

        //         // int ret=command_buf_insert_adjustment(&test_a);
        //         // printf("test_a return result is %d\r\n", ret);

        //     }
        // //     // printf("flag_2 is %d\r\n", flag_2);
        //     if(current_phase==4){
        //         if(flag==true){
        //             test_b.adjustment=input;
        //             // printf("adjust %d\r\n", test_b.adjustment);
        //             // flag=false;
        //         }
        //         // else{
        //         // // test_b.adjustment=-(d-4);
        //         // // test_b.adjustment=-2;
        //         //     test_b.adjustment=-4;
        //         //     printf("adjust %d\r\n", test_b.adjustment);
        //         //     flag=true;
        //         // }
        //         // flag_2=false;
        //         // flag_1=true;

        //         test_b.app_id=TSP.id;
        //         test_b.app_priority=TSP.priority;
        //         test_b.cycle=0;
        //         memcpy(test_b.host_OBU_id,"bus_168",7);
        //         // printf("host id is %s\r\n", test_b.host_OBU_id);
        //         test_b.phase=4;
        //         test_b.target_phase=1;
        //         test_b.effect_time=1;

        //         // int ret=command_buf_insert_adjustment(&test_b);
        //         // printf("test_a return result is %d\r\n", ret);

        //     }
        sleep(1);
    }
    // int input, temp_ack_seq;
    // while(1){
    //     printf("input function
    //     number:\r\n0:countdownoff\r\n1:countdownon\r\n2:effecttime
    //     200\r\n3:pretime\r\n4:send 5f4c\r\n5:query tc firmware version\r\n");
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
    //         // log_file_write("effect time 200\r\n");
    //         {

    //         int a,b;
    //         printf("input target phase\r\n");
    //         scanf(" %i",&b);
    //         printf("input time want to minus\r\n");
    //         scanf(" %i",&a);
    //         temp_ack_seq=tsc_dynamic();
    //         WAIT_ACK_LOOP
    //         // traffic_signal_status_t signal_status;
    //         // get_traffic_signal_status(&signal_status);
    //         int aa=47-a;
    //         printf("aa is %d\r\n", aa);
    //         temp_ack_seq=tsc_extend(b,1,aa);
    //         WAIT_ACK_LOOP
    //         }
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
