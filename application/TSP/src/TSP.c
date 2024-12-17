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
#include "application_registration.h"
#include "byte_processing.h"
#include "cms.h"
#include "config.h"
#include "error_status.h"
#include "gps_information.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "vms.h"

// extern uint8_t flag_pretime;
extern uint8_t flag_countdown_on;
extern uint8_t flag_countdown_off;
extern uint32_t activate_amount;
pthread_mutex_t file_writer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_active_TSP = PTHREAD_MUTEX_INITIALIZER;

app_obj_t TSP = {
    .name = "TSP",
    .id = TSP_ID,
    .priority = 2,
    .on_OBU_packet_rx = NULL,
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
    strncpy(command.host_OBU_name, host_OBU->OBU_name, OBU_NAME_MAX_LEN);

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
    V2R_app_section_t *app_section = (V2R_app_section_t *) arg;
    if (app_section->OBU_object->vehicle_type != VEHICLE_BUS)
        return 0;

    // printf("TSP_on_OBU_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

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
    printf("OBU_name from obu is %s\r\n", app_section->OBU_object->OBU_name);
    TSP_host_OBU_obj_t *host_OBU =
        TSP_host_OBU_obj_search(app_section->OBU_object->OBU_name);
    printf("obu distance is %d\r\n", OBU_distance);
    if (host_OBU != NULL) {
        printf("host obu id is %s and host obu targetphase is %d\r\n",
               host_OBU->OBU_name, host_OBU->target_phase);
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
        // TSP_OBU_boardcast_test(app_section->OBU_object->OBU_name);
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

    // read cmd
    uint8_t cmd;
    uint8_t ack_status = 0;
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

    char host_OBU_name[ID_MAX_LEN + 1];
    memset(host_OBU_name, 0, sizeof(host_OBU_name));
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
    case 2:  // 把動態回報的timer關掉
        set_timer(TSP_report_plan_timer_id, 0, 0, 0, 0);
        break;
    case 3:
        /* group control */
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "TSP group control\r\n");
        // read host OBU
        read_char(host_OBU_name, &read_buf, OBU_NAME_MAX_LEN);
        trim_space(host_OBU_name);
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
                strncpy(command.host_OBU_name, host_OBU_name, OBU_NAME_MAX_LEN);
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
        read_char(host_OBU_name, &read_buf, OBU_NAME_MAX_LEN);
        trim_space(host_OBU_name);
        // read target phase
        read_uint8_t(&target_phase, &read_buf);

        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ninsert host OBU (%s)", host_OBU_name);
        /* add to host OBU list */
        TSP_host_OBU_obj_insert(host_OBU_name, target_phase);
        /**   
         * record active buses if TSP service in activated (20241120新增)
         * todo: 2024.11.20 Osborn Lee
         * check if OBU if already inserted before increasing activate amount
         */
        if (TSP.dontSend2TC==0) {
            pthread_mutex_lock(&mutex_active_TSP);
            activate_amount++;
            pthread_mutex_unlock(&mutex_active_TSP); 
            log_snprintf(log_content,"active buses and EVSP in activate area: %d\n",activate_amount);
        }        
        TSP_host_OBU_obj_print();
        break;
    case 5:  // for host obu delete?
        // read host OBU
        read_char(host_OBU_name, &read_buf, OBU_NAME_MAX_LEN);
        trim_space(host_OBU_name);
        
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ndelete host OBU (%s)", host_OBU_name);
        TSP_host_OBU_obj_delete(host_OBU_name);
        TSP_host_OBU_obj_print();
        log_file_write(log_content);
        command_buf_delete_OBU(host_OBU_name);  // 刪除在 command buf 還沒下下去的指令
        command_buf_resume_control(TSP.id);     // 進行 resume 後補償
        break;
    case 6:  // disable tsp's command to tc machine
    {
        // set_timer(TSP_report_plan_timer_id, 10 / 10, 0, 1, 0);
        uint8_t enableOrdisable = 0;
        read_int8_t(&enableOrdisable, &read_buf);
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
            if (config.ped_countdown_send!=0) {
                flag_countdown_off = true;
                log_file_write("countdown is disable\r\n");
            }
            else {
                log_file_write("received flag but countdown off requested not to be sent\r\n");
            }
        } else {
            printf("Illegal command of tsc_countdown\r\n");
        }
    } break;
    // TODO: remove case 8-12, the MMP will take control.
    case 8: {
        uint8_t strategy = 0;
        uint8_t cyclenumber = 0;
        uint8_t temp_weightpi;
        read_int8_t(&strategy, &read_buf);
        read_int8_t(&cyclenumber, &read_buf);
        int temp_weightci = 0;
        int total_weight = 0;
        float phase_weight[PHASE_COUNT_MAX_NUM];

        if (strategy == 2) {
            for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
                read_int8_t(&temp_weightpi, &read_buf);
                temp_weightci = temp_weightpi;
                if (strategy == 2) {
                    total_weight = total_weight + temp_weightci;
                    phase_weight[i] = temp_weightci * 1.0;
                }
            }
            if (total_weight == 100) {
                clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
                for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
                    {
                        config.phase_weight[i] = phase_weight[i];
                        snprintf(log_content + strlen(log_content),
                                 LOG_CONTENT_LEN - strlen(log_content),
                                 "\nChange phase weight in config");
                    }
                }
            } else {
                printf("warning : total phase weight is not 100\n");
                set_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
            }
        }

        if (strategy < 4) {
            if (0 < cyclenumber && cyclenumber < 3) {
                clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
                config.traffic_compensation_cycle_number = cyclenumber;
                config.traffic_compensation_method = strategy;
                pthread_mutex_lock(&file_writer);
                FILE *outfile;
                outfile = fopen("config/config.txt", "w");
                if (outfile == NULL) {
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "\nWarning: error opening ./config/config.txt ");
                } else {
                    fprintf(outfile, "RSU_NAME \"%s\"\n", config.RSU_name);
                    fprintf(outfile, "RSU_id %d\n", config.RSU_id);
                    fprintf(outfile, "RSU_region %d\n", config.RSU_region);
                    fprintf(outfile, "RSU_LAT %f\n", config.RSU_lat);
                    fprintf(outfile, "RSU_LON %f\n", config.RSU_lon);

                    if (config.signal_controller_manufacturer == 0) {
                        fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER cheng_long\n");
                    } else if (config.signal_controller_manufacturer == 1) {
                        fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER shan_zhu\n");
                    } else {
                        fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER shan_zhu_m\n");
                    }

                    if (config.signal_status_report_active == true) {
                        fprintf(outfile, "SIGNAL_STATUS_REPORT_ACTIVE yes\n");
                    } else {
                        fprintf(outfile, "SIGNAL_STATUS_REPORT_ACTIVE no\n");
                    }

                    if (config.signal_adjust_upper_bound_active == true) {
                        fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_ACTVE yes\n");
                    } else {
                        fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_ACTVE no\n");
                    }

                    if (config.signal_adjust_lower_bound_active == true) {
                        fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_ACTVE yes\n");
                    } else {
                        fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_ACTVE no\n");
                    }

                    fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE %.0f\n", config.signal_adjust_upper_bound_percentage);
                    fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE %.0f\n", config.signal_adjust_lower_bound_percentage);
                    fprintf(outfile, "TRAFFIC_COMPENSATION_METHOD %d\n", config.traffic_compensation_method);
                    if (config.traffic_compensation_cycle_number == 1) {
                        fprintf(outfile, "TRAFFIC_COMPENSATION_CYCLE_NUMBER 1\n");
                    } else {
                        fprintf(outfile, "TRAFFIC_COMPENSATION_CYCLE_NUMBER 2\n");
                    }
                    fprintf(outfile, "PHASE_WEIGHT %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f\n", config.phase_weight[0], config.phase_weight[1], config.phase_weight[2],
                            config.phase_weight[3], config.phase_weight[4], config.phase_weight[5], config.phase_weight[6], config.phase_weight[7]);
                    if (config.log_middleware_timer_event == true) {
                        fprintf(outfile, "LOG_MIDDLEWARE_TIMER_EVENT yes\n");
                    } else {
                        fprintf(outfile, "LOG_MIDDLEWARE_TIMER_EVENT no\n");
                    }
                    if (config.log_application_register_event == true) {
                        fprintf(outfile, "LOG_APPLICATION_REGISTER_EVENT yes\n");
                    } else {
                        fprintf(outfile, "LOG_APPLICATION_REGISTER_EVENT no\n");
                    }
                    if (config.log_command_buffer == true) {
                        fprintf(outfile, "LOG_COMMAND_BUFFER yes\n");
                    } else {
                        fprintf(outfile, "LOG_COMMAND_BUFFER no\n");
                    }
                    if (config.log_signal_packet_rx == true) {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_RX yes\n");
                    } else {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_RX no\n");
                    }
                    if (config.log_signal_packet_tx == true) {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_TX yes\n");
                    } else {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_TX no\n");
                    }
                    if (config.log_signal_packet_info == true) {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_INFO yes\n");
                    } else {
                        fprintf(outfile, "LOG_SIGNAL_PACKET_INFO no\n");
                    }
                    if (config.log_cloud_packet_rx == true) {
                        fprintf(outfile, "LOG_CLOUD_PACKET_RXA yes\n");
                    } else {
                        fprintf(outfile, "LOG_CLOUD_PACKET_RXA no\n");
                    }
                    if (config.log_cloud_packet_tx == true) {
                        fprintf(outfile, "LOG_CLOUD_PACKET_TX yes\n");
                    } else {
                        fprintf(outfile, "LOG_CLOUD_PACKET_TX no\n");
                    }
                    if (config.log_OBU_packet_rx == true) {
                        fprintf(outfile, "LOG_OBU_PACKET_RX yes\n");
                    } else {
                        fprintf(outfile, "LOG_OBU_PACKET_RX no\n");
                    }
                    if (config.log_OBU_packet_tx == true) {
                        fprintf(outfile, "LOG_OBU_PACKET_TX yes\n");
                    } else {
                        fprintf(outfile, "LOG_OBU_PACKET_TX no\n");
                    }
                    if (config.log_OBU_list == true) {
                        fprintf(outfile, "LOG_OBU_LIST yes\n");
                    } else {
                        fprintf(outfile, "LOG_OBU_LIST no\n");
                    }
                }
                fclose(outfile);
                pthread_mutex_unlock(&file_writer);
            } else {
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\ninvalid cloud packet strategy tsp packet to tc "
                         "machine of invalid cyclenum: %d",
                         cyclenumber);
            }
        } else {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ninvalid cloud packet strategy tsp packet to tc "
                     "machine of invalid strategy: %d",
                     strategy);
        }
        log_file_write(log_content);
    } break;
    case 9:  // 雲端更新 VMS 圖片(要考慮到錯誤回報)
    {
        // 1.檢查檔名是否存在於VMS_pic資料夾中，無跳2.，有跳3.
        // 2.沒有指定檔案的回報處理並結束
        // 3.輪流連線四台VMS並上傳，都上傳成功則走5，反之則走4(設計成會容許失敗重傳幾次，有時候會找不到wifi，但是平均三次內都可以成功)
        // 4.上傳異常回報處理並結束
        // 5.上傳成功回報並結束
        uint8_t Program_ID;
        char Program_Name[100] = {0};
        read_uint8_t(&Program_ID, &read_buf);
        read_char(Program_Name, &read_buf, PROGRAM_NAME_LEN);
        trim_space(Program_Name);
        if (config.cms_number != 0) {
            int ret = CMS_check_img(Program_Name);
            if (ret < 0) {
                log_file_write_fatal_error("CMS_check_img(): open directory failed %s", Program_Name);
            } else {
                log_file_write("CMS_check_img(): find %s/%s success.", CMS_pic_path, Program_Name);
            }
            ret = CMS_update_activate(Program_ID, Program_Name);
            if (ret < 0) {
                log_file_write_fatal_error("cms program update is process.");
            } else {
                log_file_write("cms program update thread activate.");
            }
            ack_status = 2;
        } else {
            int res;
            // 檢查檔案存不存在資料夾中
            res = VMS_search_program(Program_Name);

            switch (res) {
            case -1: {
                log_file_write_fatal_error("VMS_search_program: open directory failed");
            } break;
            case 0: {
                if (vms_program_update_thread_activate(Program_ID, Program_Name)) {
                    log_file_write("vms program update thread activate.");
                } else {  // 正在上傳
                    log_file_write_fatal_error("vms program update is process.");
                }
                ack_status = 2;
            } break;
            case 1: {
                log_file_write_fatal_error("VMS_search_program: program doesnt exist, program name = %s", Program_Name);
                ack_status = res;
            } break;
            }
        }
    } break;
    case 10:  // 雲端更改 VMS 播放，設計成只有封包內容都正常才ACK
    {
        uint8_t VMS_ID;
        uint8_t Program_Type;
        uint8_t Program_ID;
        read_uint8_t(&VMS_ID, &read_buf);
        read_uint8_t(&Program_Type, &read_buf);
        read_uint8_t(&Program_ID, &read_buf);
        // printf("VMS_ID: %d, Program_Type: %d, Program_ID: %d\n", VMS_ID, Program_Type, Program_ID);
        log_file_write("VMS_ID: %d, Program_Type: %d, Program_ID: %d\n", VMS_ID, Program_Type, Program_ID);
        int res = carousel_update(VMS_ID, Program_Type, Program_ID);

        switch (res) {
        case 0:  // 正常
        {
            // ack_status = res;  // 初始值就是 0
        } break;
        case -1:  // VMS ID 有誤
        {
            log_file_write_fatal_error("Error VMS ID");
        } break;
        case -2:  // Program Type 有誤
        {
            log_file_write_fatal_error("Error Program Type");
        } break;
        case -3:  // Program ID 有誤
        {
            log_file_write_fatal_error("Error Program ID");
        } break;
        case -4:  // 無法開啟 vms_config.txt
        {
            log_file_write_fatal_error("Error opening vms_config.txt");
        } break;
        default: {
            log_file_write("Useless return value");
        } break;
        }

    } break;
    case 11:  // 雲端查詢 VMS 播放節目編號
    {
        VMS_report_programs_id(cmd);
    } break;
    case 12:  // 雲端查詢 VMS 編號對應檔案名稱
    {
        uint8_t program_id;
        read_uint8_t(&program_id, &read_buf);
        VMS_report_program_name(cmd, program_id);
    } break;
    case 99:  // restart daemon
    {
        char token_packet[TOKEN_LEN];
        char token[TOKEN_LEN + 1] = RESTART_TOKEN;
        read_char(token_packet, &read_buf, TOKEN_LEN);
        printf("the token recv is %s\r\n", token_packet);

        if (strncmp(token_packet, token, TOKEN_LEN) == 0) {
            TSP_send_ack(cmd, ack_status);  // 因為之後就會被 kill 了所以要在這裡傳 ack

            log_file_write("daemon get into kill self\r\n");
            printf("daemon get into kill self\r\n");
            usleep(5000);  // 等待 log 一小段時間
            kill(getpid(), SIGINT);
        }

    } break;
    default:
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nuseless tsp cmd");
        break;
    }

    TSP_send_ack(cmd, ack_status);
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
                       command->effect_time, command->host_OBU_name);
    return 0;
}

int TSP_on_registration(void *arg)
{
    /* read tsp confile file*/
    int ret = TSP_config_init();
    if (ret != 0) {
        log_file_write_fatal_error("error tsp reading config file: %d", ret);
    }

    // printf("TSP_on_registration function\n");

    /* report plan timer event */
    create_timer(&TSP_report_plan_timer_id, NULL,
                 TSP_report_plan_timer_handler);

    /* RSU_supermatrix */
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(RSU_SUPERMATRIX_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", RSU_SUPERMATRIX_DIR);
        return -1;
    } else {
        log_file_write("%s opened successfully", RSU_SUPERMATRIX_DIR);
    }

    char rsu_name[RSU_NAME_MAX_LEN];
    uint8_t plan_id;
    /* list all file */
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_name, &plan_id);
            if (strncmp(rsu_name, config.RSU_name, RSU_NAME_MAX_LEN) == 0) {
                TSP_RSU_matrix_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);
    TSP_RSU_matrix_print();

    /* OBU_supermatrix */
    if ((dp = opendir(OBU_SUPERMATRIX_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", OBU_SUPERMATRIX_DIR);
    } else {
        log_file_write("%s opened successfully", OBU_SUPERMATRIX_DIR);
    }

    /* list all file */
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_name, &plan_id);
            if (strncmp(rsu_name, config.RSU_name, RSU_NAME_MAX_LEN) == 0) {
                TSP_OBU_matrix_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);

    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, TSP.name, TSP.priority, BasicSafetyMessage_Id, &TSP_on_OBU_packet_rx);
    return 0;
}