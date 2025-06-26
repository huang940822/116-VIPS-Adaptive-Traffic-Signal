#ifndef PEDESTRIAN_TIMER_EVENT_H
#define PEDESTRIAN_TIMER_EVENT_H
#include <signal.h>
#include <bits/types/__sigset_t.h>


void PEDESTRIAN_Agent_timer_handler(__sigval_t value);

#endif