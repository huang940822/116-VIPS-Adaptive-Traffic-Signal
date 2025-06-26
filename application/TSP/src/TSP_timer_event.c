#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TSP.h"
#include "TSP_OBU_list.h"
#include "TSP_packet_tx.h"
#include "TSP_timer_event.h"
#include "com_packet_processing.h"
#include "log.h"

LOG_USE_MODULE(TSP);

timer_t TSP_report_plan_timer_id;

void TSP_report_plan_timer_handler(union sigval value)
{
    TSP_report_plan();
    return;
}

void TSP_host_OBU_list_timeout_timer_handler(union sigval value)
{
    // LOG_MSG_TRACE("TSP_host_OBU_list_timeout_timer_handler");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    LOG_MSG_APPEND(log_content, "TSP host OBU list timeout: %s",
            ((TSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);

    TSP_host_OBU_obj_delete(((TSP_host_OBU_obj_t *) value.sival_ptr)->OBU_name);
    TSP_host_OBU_obj_print();
}