#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CPS.h"
#include "EVSP.h"
#include "MMP.h"
#include "SPM.h"
#include "TIB.h"
#include "TSP.h"

#include "OBU_record_processing.h"
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
#include "vms.h"
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

    /* log init */
    log_file_init();  // 一個timer被created

    log_file_write("version : v2.4.11");

    /* read config file*/
    ret = config_init();
    if (ret != CONFIG_ACCEPT) {
        log_file_write_fatal_error("error reading config file: %d", ret);
    }
    /*read vms config file*/
    ret = vms_config_init();
    if (ret != VMS_CONFIG_ACCEPT) {
        log_file_write_fatal_error("error reading vms config file: %d", ret);
    }
    // init dsrc error detect
    dsrc_error_detect_init();
    // init tc fail detect
    tc_5fcc_error_detect_init();

    printf("query tc firmware version\r\n");
    flag_query_firm_ver = true;

    // /* taffic signal packet serial port init */
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

    pthread_t vms_thread;  // vms thread
    ret = pthread_create(&vms_thread, NULL, vms_handler, NULL);
    if (ret != 0) {
        log_file_write_fatal_error(
            "error creating vms_thread: %d", ret);
        perror("main: pthread_create");
        exit(errno);
    }

    /* command buffer init & command buffer polling timer event*/
    command_buf_init();  // 這裡面又一個timer被created

    /* traffic signal status report timer event */  // 這裡是幹麻看不懂 r2v???
    if (config.signal_status_report_active) {
        timer_t traffic_signal_status_report_timer_id;
        uint8_t traffic_signal_status_report_timer_num =
            TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT;

        create_timer(&traffic_signal_status_report_timer_id,
                     &traffic_signal_status_report_timer_num,
                     timer_event_handler);
        set_timer(traffic_signal_status_report_timer_id, 1, 0, 1, 0);
    }

    J2735Config cfg;
    ret = j2735_init(&cfg);
    if (!IS_SUCCESS(ret)) {
        printf("Fail to init J2735\n");
        return -1;
    }

    /* application service registration */
    app_obj_t *app_arr[] = {
        // &MMP,
        &EVSP,
        // &TSP,
        // &CPS,
        &TIB,
        // &SPM,
    };
    int app_arr_len = sizeof(app_arr) / sizeof(app_obj_t *);
    for (int i = 0; i < app_arr_len; i++) {
        ret = app_register(app_arr[i]);
        if (ret != 0) {
            log_file_write_fatal_error("error registering application: %d (%s)",
                                       ret, app_arr[i]->name);
        } else {
            log_file_write("%s register successfully", app_arr[i]->name);
        }
    }

    /* OBU list garbage collection timer event */  // 清掉太久的obu object
    OBU_object_garbage_collection_init();

    // log application register event
    if (config.log_application_register_event) {
        event_callback_print();
    }

    /* Packet dispatcher */
    pthread_t dispatcher_thread;
    ret = pthread_create(&dispatcher_thread, NULL, dispatcher_handler, NULL);
    if (ret != 0) {
        log_file_write_fatal_error("error creating dispatcher_thread: %d", ret);
        perror("main: pthread_create");
        exit(errno);
    }

    /* Start server */
    com_layer_init(NULL);

    while (1) {
        sleep(1);
    }
    pthread_exit(0);
    return 0;
}
