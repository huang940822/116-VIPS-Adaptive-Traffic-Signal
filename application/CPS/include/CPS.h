#ifndef CPS_H
#define CPS_H

#define DSRC_RING_MAX 4000
#include "typedefine.h"
#include "uthash.h"

extern app_obj_t CPS;
typedef struct geoinfo_table {
    double lat;
    double lon;
    float second;
} geoinfo_table;
int table_init(geoinfo_table **g_table);
int CPS_on_camera_packet_rx(void *);
int CPS_on_camera_packet_rx_performance(void *);
int CPS_on_registration(void *);
#endif