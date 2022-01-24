#ifndef CPS_H
#define CPS_H

#include "typedefine.h"
#include "uthash.h"

extern app_obj_t CPS_E;
extern app_obj_t CPS_W;
extern app_obj_t CPS_S;
extern app_obj_t CPS_N;
typedef struct geoinfo_table {           
    double lat;
    double lon;
    float second;
}geoinfo_table;
int table_init(geoinfo_table **g_table);
int CPS_on_camera_packet_rx(void *);
int CPS_on_camera_packet_rx_performance(void *);
int CPS_on_registration(void *);
#endif