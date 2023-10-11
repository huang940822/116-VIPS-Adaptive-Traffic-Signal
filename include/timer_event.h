#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H

#include "typedefine.h"

// 保證會收到回覆的 command
typedef struct Guaranteed_command_set {
    uint16_t _5F46_count;
    uint16_t _0F42_count;
} Guaranteed_command_set_t;


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

extern Guaranteed_command_set_t guarenteed_cmd_set;

#endif