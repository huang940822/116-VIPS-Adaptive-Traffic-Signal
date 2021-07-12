#ifndef TSP_H
#define TSP_H

#include "typedefine.h"

extern app_obj_t TSP;

int TSP_on_OBU_packet_rx(void *);
int TSP_on_cloud_packet_rx(void *);
int TSP_on_traffic_signal_command_tx(void *arg);
int TSP_on_registration(void *);

#endif