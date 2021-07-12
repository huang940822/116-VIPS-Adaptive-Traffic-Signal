#ifndef EVSP_TIMER_EVENT_H
#define EVSP_TIMER_EVENT_H

#include <signal.h>

#define EVSP_HOST_OBU_PACKET_TIMEOUT 20
#define EVSP_HOST_OBU_LIST_TIMEOUT 120

void EVSP_host_OBU_packet_timeout_timer_handler(union sigval value);
void EVSP_host_OBU_list_timeout_timer_handler(union sigval value);

#endif