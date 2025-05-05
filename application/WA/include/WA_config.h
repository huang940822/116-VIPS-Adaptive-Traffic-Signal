#ifndef WA_CONFIG_H
#define WA_CONFIG_H

#include "typedefine.h"
#include <stdint.h>
#define WA_CONFIG_FILE FILE_PATH "application/WA/config/config.txt"
#define MAX_SIGNAL_COUNT 8


int WA_config_init();

typedef enum WA_config_err {
    WA_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_WA_WARNING_RANGE = -1,
    CONFIG_INVALID_WA_WARNING_FREQ = -2,
    CONFIG_INVALID_WA_INTERSECTION_CENTER_LAT = -3,
    CONFIG_INVALID_WA_INTERSECTION_CENTER_LON = -4,
    CONFIG_INVALID_WA_PACKET = -5,
    CONFIG_INVALID_WA_OPEN_FILE = -6,
    CONFIG_INVALID_WA_BRANCH2MAIN = -7,
    CONFIG_INVALID_WA_BRANCH2MAINWARNINGRANGE = -8,
    CONFIG_INVALID_WA_MAINDIRECTION = -9
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
    int warning_range;
    double warning_freq;
    int branch2main;
    int branch2mainRange;
    int maindirection;
    double intersection_center_lat;
    double intersection_center_lon;
} WA_config_object_t;

extern WA_config_object_t WA_config;


#endif