#ifndef MAP_H
#define MAP_H

#include <typedefine.h>

extern app_obj_t MAP;

int MAP_on_registration(void *);
int MAP_on_CLOUD_packet_rx(void *);

#endif