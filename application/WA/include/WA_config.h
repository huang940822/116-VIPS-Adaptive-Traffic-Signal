#ifndef WA_CONFIG_H
#define WA_CONFIG_H

#include "typedefine.h"
#include <stdint.h>
#define WA_CONFIG_FILE FILE_PATH "application/WA/config/config.txt"
#define MAX_SIGNAL_COUNT 8


int WA_config_init();

typedef enum WA_config_err {
    WA_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_WA_PACKET = -1,
    CONFIF_INVALID_WA_OPEN_FILE = -2
} WA_config_err_t;
typedef struct traffic_light_lat_lon {
    double Traffic_light_lat;
    double Traffic_light_lon;
} traffic_light_lat_lon_t;
typedef struct traffic_light_set{
    traffic_light_lat_lon_t *tab;
    int intersection_count;
} traffic_light_set_t;
typedef struct WA_config_object {
    traffic_light_set_t traffic_light;
} WA_config_object_t;

extern WA_config_object_t WA_config;


#endif