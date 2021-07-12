#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "com_io.h"

extern uint8_t cloud_com_id;
extern uint8_t OBU_com_id;

void* dispatcher_handler();

#endif