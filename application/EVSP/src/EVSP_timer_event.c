#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "EVSP_OBU_list.h"
#include "EVSP_packet_tx.h"
#include "EVSP_touching_area.h"
#include "cms.h"
#include "config.h"
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

    if (config.cms_number != 0) {
        CMS_request_end(EVSP.id);
    } else {
        // 檢查 EVSP 有沒有關掉 VMS 服務，沒有的話要關掉
        vms_request_end(EVSP.id);
    }

    printf("EVSP_host_OBU_packet_timeout_timer_handler\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    log_snprintf(log_content, "EVSP host OBU packet timeout: %s",
                 ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);
    int target_phase = ((EVSP_host_OBU_obj_t *) value.sival_ptr)->target_phase;

    command_buf_delete_OBU(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);
    EVSP_host_OBU_obj_delete(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(target_phase) == true) {
        command_buf_resume_control(EVSP.id);
    }
    log_file_write(log_content);

    EVSP_host_OBU_obj_print();
}

void EVSP_host_OBU_list_timeout_timer_handler(union sigval value)
{
    // 回報 EVSP timeout event
    EVSP_timeout_report(value);

    if (config.cms_number != 0) {
        CMS_request_end(EVSP.id);
    } else {
        // 檢查 EVSP 有沒有關掉 VMS 服務，沒有的話要關掉
        vms_request_end(EVSP.id);
    }

    printf("EVSP_host_OBU_list_timeout_timer_handler\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    log_snprintf(log_content, "EVSP host OBU list timeout: %s",
                 ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    int target_phase = ((EVSP_host_OBU_obj_t *) value.sival_ptr)->target_phase;

    command_buf_delete_OBU(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);
    EVSP_host_OBU_obj_delete(((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(target_phase) == true) {
        command_buf_resume_control(EVSP.id);
    }
    log_file_write(log_content);

    EVSP_host_OBU_obj_print();
}