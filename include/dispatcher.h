#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "com_io.h"
#define THREAD 32
#define THREADQUEUE 14400

extern uint8_t cloud_com_id;
extern uint8_t OBU_com_id;
extern uint8_t Heartbeat_com_id;
void *dispatcher_handler();

#endif