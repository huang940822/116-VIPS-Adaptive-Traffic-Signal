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
    CONFIG_INVALID_WA_COLLECTORTOARTERIAL = -7,
    CONFIG_INVALID_WA_COLLECTORTOARTERIALWARNINGRANGE = -8,
    CONFIG_INVALID_WA_MAINDIRECTION = -9
} WA_config_err_t;
typedef struct WA_config_object {
    int warning_range;
    double warning_freq;
    int collector_to_arterial;
    int collector_to_arterial_warning_range;
    int maindirection;
    double intersection_center_lat;
    double intersection_center_lon;
} WA_config_object_t;

extern WA_config_object_t WA_config;


#endif