#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include "typedefine.h"
#include <time.h>
#include <sys/types.h>

void timer_event_handler();
int create_timer(timer_t *timer_id,
                 void *signal_value,
                 void (*notify_function)());
int set_timer(timer_t timer_id,
              int32_t interval_sec,
              int32_t interval_nsec,
              uint32_t initial_sec,
              uint32_t initial_nsec);
int delete_timer(timer_t timer_id);

#endif  /* TIMER_EVENT_H */