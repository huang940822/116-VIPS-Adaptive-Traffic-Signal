#ifndef TIB_H
#define TIB_H

#include <typedefine.h>

extern app_obj_t TIB;

int TIB_on_registration(void *);
int TIB_on_CLOUD_packet_rx(void *);
int TIB_on_traffic_signal_command_tx(void *arg);

#endif