#ifndef EVSP_H
#define EVSP_H

#include "typedefine.h"

extern app_obj_t EVSP;

extern app_obj_t EVSP1;
extern app_obj_t EVSP2;
extern app_obj_t EVSP3;
extern app_obj_t EVSP4;
extern app_obj_t EVSP5;

int EVSP_on_OBU_packet_rx(void *);
int EVSP_on_CLOUD_packet_rx(void *);
int EVSP_on_registration(void *);

#endif