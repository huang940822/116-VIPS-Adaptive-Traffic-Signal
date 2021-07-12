#ifndef TSP_TIMER_EVENT_H
#define TSP_TIMER_EVENT_H

#include <signal.h>

#define TSP_HOST_OBU_LIST_TIMEOUT 120

extern timer_t TSP_report_plan_timer_id;

void TSP_report_plan_timer_handler(union sigval value);
void TSP_host_OBU_list_timeout_timer_handler(union sigval value);

#endif