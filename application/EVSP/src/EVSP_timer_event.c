#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "EVSP_OBU_list.h"
#include "EVSP_touching_area.h"
#include "EVSP_packet_tx.h"
#include "log.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "vms.h"

void EVSP_timeout_report(union sigval value)
{
    // 回報 timeout event 需要的變數資訊
    OBU_object_t timeout_obu_obj = {0};
    strncpy(timeout_obu_obj.OBU_name, 
            ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name,
            OBU_NAME_MAX_LEN);
    timeout_obu_obj.vehicle_type = VEHICLE_AMBULANCE;
    
    // 回報 EVSP timeout event
    EVSP_report_activate_area(&timeout_obu_obj, TIMEOUT, 0);
}

void EVSP_host_OBU_packet_timeout_timer_handler(union sigval value)
{   
    // 回報 EVSP timeout event
    EVSP_timeout_report(value);

    // 檢查 EVSP 有沒有關掉 VMS 服務，沒有的話要關掉
    vms_request_end(EVSP.id);

    printf("EVSP_host_OBU_packet_timeout_timer_handler\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP host OBU packet timeout: %s",
             ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = EVSP.id;
    command.app_priority = EVSP.priority;
    command.target_phase =
        ((EVSP_host_OBU_obj_t *) value.sival_ptr)->target_phase;
    strncpy(command.host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN);

    EVSP_host_OBU_obj_delete(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(command.target_phase) == true) {
        uint8_t current_phase = signal_status.SubPhaseID;
        int ret = 0;
        //看不懂
        if (command.target_phase >= current_phase) {
            command.cycle = 0;
            command.phase = command.target_phase;
            command.effect_time =
                signal_status.plan[command.target_phase - 1].PreGreen;
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d (%d)",
                     command.cycle, command.phase, command.effect_time, ret);
        } else {
            command.cycle = 1;
            command.phase = command.target_phase;
            command.effect_time =
                signal_status.plan[command.target_phase - 1].PreGreen;
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d (%d)",
                     command.cycle, command.phase, command.effect_time, ret);
        }
    }
    log_file_write(log_content);

    EVSP_host_OBU_obj_print();
}

void EVSP_host_OBU_list_timeout_timer_handler(union sigval value)
{   
    // 回報 EVSP timeout event
    EVSP_timeout_report(value);
    
    // 檢查 EVSP 有沒有關掉 VMS 服務，沒有的話要關掉
    vms_request_end(EVSP.id);

    printf("EVSP_host_OBU_list_timeout_timer_handler\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP host OBU list timeout: %s",
             ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = EVSP.id;
    command.app_priority = EVSP.priority;
    command.target_phase =
        ((EVSP_host_OBU_obj_t *) value.sival_ptr)->target_phase;
    strncpy(command.host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN);

    EVSP_host_OBU_obj_delete(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(command.target_phase) == true) {
        uint8_t current_phase = signal_status.SubPhaseID;
        int ret = 0;
        if (command.target_phase >= current_phase) {
            command.cycle = 0;
            command.phase = command.target_phase;
            command.effect_time =
                signal_status.plan[command.target_phase - 1].PreGreen;
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d (%d)",
                     command.cycle, command.phase, command.effect_time, ret);
        } else {
            command.cycle = 1;
            command.phase = command.target_phase;
            command.effect_time =
                signal_status.plan[command.target_phase - 1].PreGreen;
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d (%d)",
                     command.cycle, command.phase, command.effect_time, ret);
        }
    }
    log_file_write(log_content);

    EVSP_host_OBU_obj_print();
}