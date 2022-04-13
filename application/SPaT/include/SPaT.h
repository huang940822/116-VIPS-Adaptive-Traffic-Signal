#ifndef SPAT_H
#define SPAT_H

#include <typedefine.h>

extern app_obj_t SPaT;

int SPaT_on_registration(void *);
int SPaT_on_CLOUD_packet_rx(void *);

#endif