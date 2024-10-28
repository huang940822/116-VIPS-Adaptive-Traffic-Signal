#ifndef TIMER_EVENT_H
#define TIMER_EVENT_H
#include "ae_event.h"
#include "msg_queue.h"
#define SEC_TO_MSEC(sec) (640 * sec)

int time_print_cur_time(ae_event_loop *event_loop,
                        long long id,
                        void *clientData);
int on_cloud_disconnected(ae_event_loop *event_loop,
                          long long id,
                          void *clientData);
#endif