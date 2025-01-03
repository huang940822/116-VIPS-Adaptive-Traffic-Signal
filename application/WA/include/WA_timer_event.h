#ifndef WA_TIMER_EVENT_H
#define WA_TIMER_EVENT_H
#include <signal.h>
#include <bits/types/__sigval_t.h>


void WA_Agent_timer_handler(__sigval_t value);

#endif