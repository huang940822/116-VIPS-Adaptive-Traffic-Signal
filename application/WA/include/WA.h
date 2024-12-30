#ifndef WA_H
#define WA_H

#include "typedefine.h"

extern app_obj_t WA;

typedef struct{
    double speed;
    double distance;
}CCI;

int WA_on_camera_packet_rx(void *);
int WA_on_registration(void *);

#endif