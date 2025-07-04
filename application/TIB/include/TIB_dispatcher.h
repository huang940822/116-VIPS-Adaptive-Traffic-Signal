#ifndef TIB_DISPATCHER_H
#define TIB_DISPATCHER_H

#define THREAD 2
#define THREADQUEUE 3600
#include "typedefine.h"
#include "j2735_BroadcastList.h"
typedef struct J2735_msg_obj J2735_msg_obj_t;
timer_t TIB_report_plan_timer_id;
void TIB_dispatcher_handler();
void on_j2735_msg_ready();
void process_external_message(J2735_msg_obj_t *external_J2735_msg, int cycles);
#endif