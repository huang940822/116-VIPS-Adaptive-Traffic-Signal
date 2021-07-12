#ifndef EVSP_H
#define EVSP_H

#include "typedefine.h"

extern app_obj_t EVSP;

int EVSP_on_OBU_packet_rx(void *);
int EVSP_on_CLOUD_packet_rx(void *);
int EVSP_on_registration(void *);

#endif