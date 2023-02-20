#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "config.h"
#include "TSP.h"
#include "TSP_matrix.h"
#include "TSP_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

void TSP_send_ack(uint8_t cmd)
{
    printf("tsp send ack, CMD is %d\r\n", cmd);
    log_file_write("tsp send ack, CMD is %d\r\n", cmd);
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_send_ack: malloc");
        perror("TSP_send_ack: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(cmd, &write_buf);
    write_uint8_t(cmd, &write_buf);

    cloud_packet_tx(write_buf.index, TSP.id, write_buf.content);
    free(write_buf.content);
    return;
}

void TSP_report_plan()
{
    printf("report tsp plan\r\n");
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_report_plan: malloc");
        perror("TSP_report_plan: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    uint8_t error_status = get_error_status();
    uint16_t original_tc_health_status = get_original_tc_health_status();
    // cmd
    write_uint8_t(1, &write_buf);

    // signal_status
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
    // control strategy
    write_uint8_t(signal_status.ControlStrategy, &write_buf);
    // error status
    write_uint8_t(error_status, &write_buf);
    write_uint16_t(original_tc_health_status,&write_buf);
    write_uint8_t(config.traffic_compensation_method,&write_buf);
    
    cloud_packet_tx(write_buf.index, TSP.id, write_buf.content);
    free(write_buf.content);
    return;
}

void TSP_report_command(uint8_t control_status,
                        uint8_t sub_phase_id,
                        uint8_t step_id,
                        uint8_t effect_time,
                        char *OBU_name)
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_report_command: malloc");
        perror("TSP_report_command: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(2, &write_buf);

    write_uint8_t(control_status, &write_buf);
    write_uint8_t(sub_phase_id, &write_buf);
    write_uint8_t(step_id, &write_buf);
    write_uint8_t(effect_time, &write_buf);
    write_char(OBU_name, &write_buf, OBU_NAME_MAX_LEN - 1, OBU_NAME_MAX_LEN);


    cloud_packet_tx(write_buf.index, TSP.id, write_buf.content);
    free(write_buf.content);
    return;
}
//用來把收到的obu資訊廣播出去嗎？ 目前沒用到
void TSP_OBU_boardcast(TSP_host_OBU_obj_t *host_OBU)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "TSP info boardcast:");

    // search plan
    uint8_t plan_id = get_plan_id();
    TSP_OBU_matrix_t *matrix = TSP_OBU_matrix_search(plan_id);

    uint16_t distance_index =
        host_OBU->distance / TSP_REMAINING_DISTANCE_INTERVAL;
    distance_index = (distance_index >= TSP_REMAINING_DISTANCE_NUM)
                         ? (TSP_REMAINING_DISTANCE_NUM - 1)
                         : (distance_index);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

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

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_OBU_boardcast: malloc");
        perror("TSP_OBU_boardcast: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // OBU_name
    write_char(host_OBU->OBU_name, &write_buf, OBU_NAME_MAX_LEN, OBU_NAME_MAX_LEN);
    // passing rate
    uint8_t passing_rate = matrix
                               ->entry[distance_index][signal_phase_index]
                                      [time_index][target_phase_index]
                               .passing_rate;
    write_uint8_t(passing_rate, &write_buf);
    // recommend speed
    uint8_t recommend_speed = matrix
                                  ->entry[distance_index][signal_phase_index]
                                         [time_index][target_phase_index]
                                  .recommend_speed;
    write_uint8_t(recommend_speed, &write_buf);

    OBU_packet_tx(write_buf.index, TSP.id, write_buf.content);
    if (write_buf.content != NULL) {
        free(write_buf.content);
    }
    log_file_write(log_content);
    return;
}