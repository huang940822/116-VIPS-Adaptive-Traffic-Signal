#ifndef APP_H
#define APP_H

#include "typedefine.h"
#include "uthash.h"

extern app_obj_t APP;

int APP_on_camera_packet_rx(void *);
int APP_on_registration(void *);
void handler(int );
#endif