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

LOG_USE_MODULE(EVSP);

void EVSP_timeout_report(union sigval value)
{
    // 回報 timeout event 需要的變數資訊
    OBU_object_t timeout_obu_obj = {0};
    strncpy(timeout_obu_obj.OBU_name,
            ((EVSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name,
            OBU_NAME_MAX_LEN);
    timeout_obu_obj.vehicle_type = ((EVSP_host_OBU_obj_t *) value.sival_ptr)->vehicle_type;

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

    LOG_MSG_TRACE("EVSP_host_OBU_packet_timeout_timer_handler");
    EVSP_host_OBU_obj_t *obu_obj = (EVSP_host_OBU_obj_t *) value.sival_ptr;
    int target_phase = obu_obj->target_phase;

    command_buf_delete_OBU(obu_obj->OBU_name);
    EVSP_cooling_list_insert(obu_obj->OBU_name, obu_obj->area_ptr);
    EVSP_host_OBU_obj_delete(obu_obj->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(target_phase) == true) {
        command_buf_resume_control(EVSP.id);
    }

    LOG_MSG_WARN("EVSP host OBU packet timeout: %s", obu_obj->OBU_name);
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

    LOG_MSG_TRACE("EVSP_host_OBU_list_timeout_timer_handler");
    EVSP_host_OBU_obj_t *obu_obj = (EVSP_host_OBU_obj_t *) value.sival_ptr;
    int target_phase = obu_obj->target_phase;

    command_buf_delete_OBU(obu_obj->OBU_name);
    EVSP_cooling_list_insert(obu_obj->OBU_name, obu_obj->area_ptr);
    EVSP_host_OBU_obj_delete(obu_obj->OBU_name);

    // no other host OBU with same target phase in host_OBU_list
    if (EVSP_host_OBU_obj_resume(target_phase) == true) {
        command_buf_resume_control(EVSP.id);
    }

    LOG_MSG_WARN("EVSP host OBU list timeout: %s", obu_obj->OBU_name);
    EVSP_host_OBU_obj_print();
}