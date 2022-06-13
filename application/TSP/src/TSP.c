#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "TSP.h"
#include "TSP_OBU_list.h"
#include "TSP_config.h"
#include "TSP_matrix.h"
#include "TSP_packet_tx.h"
#include "TSP_timer_event.h"
#include "TSP_typedefine.h"
#include "byte_processing.h"
#include "config.h"
#include "error_status.h"
#include "gps_information.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"

// extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;

app_obj_t TSP = {
    .name = "TSP",
    .id = 2,
    .priority = 2,
    .on_OBU_packet_rx = &TSP_on_OBU_packet_rx,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &TSP_on_cloud_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_traffic_signal_command_tx = &TSP_on_traffic_signal_command_tx,
    .on_registration = &TSP_on_registration,
    .next = NULL,
    .dontSend2TC = 1,
};

void TSP_supermatrix_lookup(TSP_host_OBU_obj_t *host_OBU)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "TSP supermatrix lookup:");

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nSubPhaseID is 0");
        log_file_write(log_content);
        return;
    }

    // search plan
    uint8_t plan_id = get_plan_id();
    TSP_RSU_matrix_t *matrix = TSP_RSU_matrix_search(plan_id);

    if (matrix == NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\nRSU matrix not found");
        log_file_write(log_content);
        return;
    }

    uint16_t distance_index =
        host_OBU->distance / TSP_REMAINING_DISTANCE_INTERVAL;
    distance_index = (distance_index >= TSP_REMAINING_DISTANCE_NUM)
                         ? (TSP_REMAINING_DISTANCE_NUM - 1)
                         : (distance_index);

    // 閃光時會出現 SubPhaseID = 0
    uint8_t current_phase = signal_status.SubPhaseID;
    uint8_t signal_phase_index =
        (current_phase - 1) / TSP_SIGNAL_PHASE_INTERVAL;
    signal_phase_index = (signal_phase_index < 0) ? 0 : (signal_phase_index);

    uint16_t remaining_time = get_remaining_time(
        signal_status.SubPhaseID, signal_status.StepID, signal_status.StepSec);
    uint16_t time_index = remaining_time / TSP_REMAINING_TIME_INTERVAL;
    time_index = (time_index >= TSP_REMAINING_TIME_NUM)
                     ? (TSP_REMAINING_TIME_NUM - 1)
                     : (time_index);

    uint8_t target_phase_index =
        (host_OBU->target_phase - 1) / TSP_TARGET_PHASE_INTERVAL;

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "\ndistance:     %u (%d)",
             host_OBU->distance, distance_index);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "\nsubphase:     %u (%d)",
             current_phase, signal_phase_index);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "\nremaining:    %u (%d)",
             remaining_time, time_index);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "\ntarget phase: %u (%d)",
             host_OBU->target_phase, target_phase_index);

    /* control status - intersection control */
    int ret = 0;
    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = TSP.id;
    command.app_priority = TSP.priority;
    command.target_phase = host_OBU->target_phase;
    strncpy(command.host_OBU_id, host_OBU->OBU_id, OBU_ID_MAX_LEN);

    for (int i = 0; i < TSP_CYCLE_MAX; i++) {
        for (int j = 0; j < TSP_PHASE_MAX; j++) {
            command.cycle = i;
            command.phase = j + 1;
            command.adjustment = matrix
                                     ->entry[distance_index][signal_phase_index]
                                            [time_index][target_phase_index]
                                     .adjustment[i][j];
            // if(TSP.dontSend2TC==0){
            ret = command_buf_insert_adjustment(&command);
            // }
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, adjustment: %d (%d)", i, j + 1,
                     matrix
                         ->entry[distance_index][signal_phase_index][time_index]
                                [target_phase_index]
                         .adjustment[i][j],
                     ret);
        }
    }
    log_file_write(log_content);
    return;
}

int TSP_on_OBU_packet_rx(void *arg)
{
    // printf("TSP_on_OBU_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    V2R_app_section_t *app_section = (V2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_on_OBU_packet_rx: malloc");
        perror("TSP_on_OBU_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }

    /* print packet */
    if (config.log_OBU_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "TSP OBU packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    TSP_static_space_t static_space;
    read_uint8_t(&static_space.on_duty_flag, &read_buf);
    read_uint8_t(&static_space.passenger_num, &read_buf);
    memcpy(app_section->OBU_object->private_space[TSP.id].static_space,
           &static_space, sizeof(TSP_static_space_t));

    uint8_t last_record_index =
        app_section->OBU_object->record_ring.last_record_pointer;
    uint16_t OBU_distance = (uint16_t) get_distance(
        config.RSU_lat, config.RSU_lon,
        app_section->OBU_object->record_ring.record[last_record_index]
            .position_lat,
        app_section->OBU_object->record_ring.record[last_record_index]
            .position_lon);

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "TSP OBU packet rx: OBU POSITION");
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\nlat, lon: %f, %f\nOBU distance: %hd",
             app_section->OBU_object->record_ring.record[last_record_index]
                 .position_lat,
             app_section->OBU_object->record_ring.record[last_record_index]
                 .position_lon,
             OBU_distance);
    log_file_write(log_content);
    printf("obu_id from obu is %s\r\n", app_section->OBU_object->OBU_id);
    TSP_host_OBU_obj_t *host_OBU =
        TSP_host_OBU_obj_search(app_section->OBU_object->OBU_id);
    printf("obu distance is %d\r\n", OBU_distance);
    if (host_OBU != NULL) {
        printf("host obu id is %s and host obu targetphase is %d\r\n",
               host_OBU->OBU_id, host_OBU->target_phase);
    }


    // if (host_OBU != NULL && OBU_distance < TSP_REMAINING_DISTANCE_MAX) {
    if (host_OBU != NULL &&
        OBU_distance < TSP_config.tsp_remaining_distance_max) {
        printf("tsp supermatrix lookup\r\n");
        host_OBU->distance = OBU_distance;
        TSP_supermatrix_lookup(host_OBU);
        // TSP_OBU_boardcast(host_OBU);
    } else {
        // testing
        // TSP_OBU_boardcast_test(app_section->OBU_object->OBU_id);
    }
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}


int TSP_on_cloud_packet_rx(void *arg)
{
    printf("\nTSP_on_cloud_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    // typedef struct C2R_app_section {
    //     uint32_t payload_len;
    //     char *payload;
    //     uint8_t com_id;
    // } C2R_app_section_t;
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_on_cloud_packet_rx: malloc");
        perror("TSP_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }

    TSP_send_ack();

    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "TSP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
        log_file_write(log_content);
    }

    char host_OBU_id[OBU_ID_MAX_LEN + 1];
    memset(host_OBU_id, 0, sizeof(host_OBU_id));
    uint8_t target_phase;
    uint16_t frequency;
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "TSP cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0:
        TSP_report_plan();
        break;
    case 1:  // change the frequency(time interval) of report plan by set_timer
        read_uint16_t(&frequency, &read_buf);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nfrequency: %d",
                 frequency);
        set_timer(TSP_report_plan_timer_id, frequency / 10, 0, 1, 0);
        break;
    case 2:  //把動態回報的timer關掉
        set_timer(TSP_report_plan_timer_id, 0, 0, 0, 0);
        break;
    case 3:
        /* group control */
        snprintf(log_content + strlen(log_content),
                LOG_CONTENT_LEN - strlen(log_content),
                "TSP group control\r\n");
        // read host OBU
        read_char(host_OBU_id, &read_buf, OBU_ID_MAX_LEN);
        // read target phase
        read_uint8_t(&target_phase, &read_buf);

        uint8_t cycle;
        uint8_t phase;
        int8_t adjustment;
        tsc_command_t command;
        memset(&command, 0, sizeof(tsc_command_t));
        int ret = 0;
        for (int i = 0; i < TSP_PHASE_MAX * TSP_CYCLE_MAX; i++) {
            // read cycle
            read_uint8_t(&cycle, &read_buf);
            // read phase
            read_uint8_t(&phase, &read_buf);
            // read adjustment
            read_int8_t(&adjustment, &read_buf);
            if (cycle != 0 || phase != 0 || adjustment != 0) {
                command.app_id = TSP.id;
                command.app_priority = TSP.priority;
                command.target_phase = target_phase;
                command.cycle = cycle;
                command.phase = phase;
                command.adjustment = adjustment;
                strncpy(command.host_OBU_id, host_OBU_id, OBU_ID_MAX_LEN);
                ret = command_buf_insert_adjustment(&command);


                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\ncycle: %d, phase: %d, adjustment: %d (%d)", cycle,
                         phase, adjustment, ret);
            }
        }
        break;
    case 4:
        /* intersection control */
        // read host OBU
        read_char(host_OBU_id, &read_buf, OBU_ID_MAX_LEN);
        // read target phase
        read_uint8_t(&target_phase, &read_buf);

        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ninsert host OBU (%s)", host_OBU_id);
        /* add to host OBU list */
        TSP_host_OBU_obj_insert(host_OBU_id, target_phase);
        TSP_host_OBU_obj_print();
        break;
    case 5:  // for host obu delete?
        // read host OBU
        read_char(host_OBU_id, &read_buf, OBU_ID_MAX_LEN);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ndelete host OBU (%s)", host_OBU_id);
        TSP_host_OBU_obj_delete(host_OBU_id);
        TSP_host_OBU_obj_print();
        for (int i = 0; i < SUBPHASEID_NUM; i++) {
            // printf("compensation_buffer[%d]:%d\r\n",i,compensation_buffer[i]);
            snprintf(log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "compensation_buffer[%d]:%d\r\n", i,
                    compensation_buffer[i]);
        }
        log_file_write(log_content);
        // 進行補償
        switch (config.traffic_compensation_method) {
        case 1:
            traffic_compensation_method1();
            break;
        case 2:
            traffic_compensation_method2();
            break;
        case 3:
            traffic_compensation_method3();
            break;
        default:
            break;
        }
        break;
    case 6:  // disable tsp's command to tc machine
    {
        // set_timer(TSP_report_plan_timer_id, 10 / 10, 0, 1, 0);
        uint8_t enableOrdisable = 0;
        uint8_t type = 0;
        read_int8_t(&enableOrdisable, &read_buf);
        read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            TSP.dontSend2TC == 0) {  // enable/clear command buffer
            TSP.dontSend2TC = 1;
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\ntsp disable");
            printf("tsp disable\r\n");
        } else if (enableOrdisable == 2 &&
                   TSP.dontSend2TC ==
                       1) {  // disable command buffer/then stop the command in
                             // command buffer sent to tc machine
            TSP.dontSend2TC = 0;
            command_buf_clear();
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ntsp enable and clear command buffer");
            printf("tsp enable\r\n");
        } else {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ninvalid cloud packet disable/enable tsp packet to tc "
                     "machine");
        }
    } break;
    case 7:  // disalbe/enable tsc_countdown
    {
        uint8_t enableOrdisable = 0;
        // uint8_t tc_machine_type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&tc_machine_type, &read_buf);

        // enable
        if (enableOrdisable == 2) {
            // todo: write a api to let app get the config in middleware but let
            // the variable all exposed to app?
            // tsc_countdown_on(config.signal_controller_manufacturer);
            flag_countdown_on = true;
            log_file_write("countdown is enable\r\n");
        } else if (enableOrdisable == 1) {  // disable
            // tsc_countdown_off(config.signal_controller_manufacturer);
            flag_countdown_off = true;
            log_file_write("countdown is disable\r\n");
        } else {
            printf("Illegal command of tsc_countdown\r\n");
        }
    } break;
    case 99:  // restart daemon
    {
        char token_packet[TOKEN_LEN];
        char token[TOKEN_LEN + 1] = RESTART_TOKEN;
        read_char(token_packet, &read_buf, TOKEN_LEN);
        printf("the token recv is %s\r\n", token_packet);

        if (strncmp(token_packet, token, TOKEN_LEN) == 0) {
            log_file_write("daemon get into kill self\r\n");
            printf("daemon get into kill self\r\n");
            kill(getpid(), SIGINT);
        }

    } break;
    default:
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nuseless tsp cmd");

        break;
    }

    log_file_write(log_content);

    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

int TSP_on_traffic_signal_command_tx(void *arg)
{
    // printf("TSP_on_traffic_signal_command_tx\n");
    traffic_signal_command_arg_t *command =
        (traffic_signal_command_arg_t *) arg;
    // 1-2-T-2
    TSP_report_command(command->control_status, command->phase, command->step,
                       command->effect_time, command->host_OBU_id);
    return 0;
}

int TSP_on_registration(void *arg)
{
    // printf("TSP_on_registration function\n");

    /* report plan timer event */
    create_timer(&TSP_report_plan_timer_id, NULL,
                 TSP_report_plan_timer_handler);

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    /* RSU_supermatrix */
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(RSU_SUPERMATRIX_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", RSU_SUPERMATRIX_DIR);
    } else {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s opened successfully", RSU_SUPERMATRIX_DIR);
        log_file_write(log_content);
    }

    char rsu_id[RSU_ID_MAX_LEN];
    uint8_t plan_id;
    /* list all file */
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_id, &plan_id);
            if (strncmp(rsu_id, config.RSU_id, RSU_ID_MAX_LEN) == 0) {
                TSP_RSU_matrix_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);
    TSP_RSU_matrix_print();

    memset(log_content, 0, sizeof(log_content));

    /* OBU_supermatrix */
    if ((dp = opendir(OBU_SUPERMATRIX_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", OBU_SUPERMATRIX_DIR);
    } else {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s opened successfully", OBU_SUPERMATRIX_DIR);
        log_file_write(log_content);
    }

    /* list all file */
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_id, &plan_id);
            if (strncmp(rsu_id, config.RSU_id, RSU_ID_MAX_LEN) == 0) {
                TSP_OBU_matrix_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);
    return 0;
}