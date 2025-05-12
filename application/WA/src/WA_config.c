#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WA_config.h"
#include "config.h"
#include "log.h"

LOG_USE_MODULE(WA);

WA_config_object_t WA_config ={};
static bool read_float_from_config_line(char *config_line, float *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %f\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}


static bool read_int_from_config_line(char *config_line, int *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %d\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

int WA_config_init(){
    FILE *fp;
    fp = fopen(WA_CONFIG_FILE, "r");
    if (fp == NULL) {
        LOG_MSG_ERROR("error opening %s", WA_CONFIG_FILE);
        return CONFIG_INVALID_WA_OPEN_FILE;
    } else {
        LOG_MSG_INFO("%s opened successfully", WA_CONFIG_FILE);
    }
    
    char read_buf[CONFIG_LINE_BUFFER_SIZE];
    memset(read_buf, 0, sizeof(read_buf));

    int val;
    float float_val;
    char string_val[MAX_CONFIG_VARIABLE_LEN];

    WA_config.traffic_light.tab = malloc(MAX_SIGNAL_COUNT * sizeof(traffic_light_lat_lon_t));

    while(!feof(fp)) {
        fgets(read_buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (read_buf[0] == '#' || read_buf[0] == '\n' || read_buf[0] == ' '){
            continue;
        }
        // WARNING_RANGE
        if (strstr(read_buf, "WA_WARNING_RANGE")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.warning_range = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_WARNING_RANGE;
                }
            } else {
                return CONFIG_INVALID_WA_WARNING_RANGE;
            } 
        }
        // WARNING_FREQ
        if (strstr(read_buf, "WA_WARNING_FREQ")) {
            if (read_float_from_config_line(read_buf, &float_val)) {
                if (float_val >= 0) {
                    WA_config.warning_freq = float_val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_WARNING_FREQ;
                }
            } else {
                return CONFIG_INVALID_WA_WARNING_FREQ;
            }
        }

        // COLLECTOR_TO_ARTERIAL_WARNING_RANGE
        if (strstr(read_buf, "COLLECTOR_TO_ARTERIAL_WARNING_RANGE")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.collector_to_arterial_warning_range = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_COLLECTORTOARTERIALWARNINGRANGE;
                }
            } else {
                return CONFIG_INVALID_WA_COLLECTORTOARTERIALWARNINGRANGE;
            }
        }

        // COLLECTOR TO ARTERIAL
        if (strstr(read_buf, "COLLECTOR_TO_ARTERIAL")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.collector_to_arterial = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_COLLECTORTOARTERIAL;
                }
            } else {
                return CONFIG_INVALID_WA_COLLECTORTOARTERIAL;
            }
        }

        // MAIN_DIRECTION
        if (strstr(read_buf, "MAIN_DIRECTION")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.maindirection = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_MAINDIRECTION;
                }
            } else {
                return CONFIG_INVALID_WA_MAINDIRECTION;
            }
        }



        // INTERSECTION_CENTER_LAT
        if (strstr(read_buf, "INTERSECTION_CENTER_LAT")) {
            if (read_float_from_config_line(read_buf, &float_val)) {
                if (float_val >= 0) {
                    WA_config.intersection_center_lat = float_val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_INTERSECTION_CENTER_LAT;
                }
            } else {
                return CONFIG_INVALID_WA_INTERSECTION_CENTER_LAT;
            }
        }
        // INTERSECTION_CENTER_LON
        if (strstr(read_buf, "INTERSECTION_CENTER_LON")) {
            if (read_float_from_config_line(read_buf, &float_val)) {
                if (val >= 0) {
                    WA_config.intersection_center_lon = float_val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_INTERSECTION_CENTER_LON;
                }
            } else {
                return CONFIG_INVALID_WA_INTERSECTION_CENTER_LON;
            }
        }
    } 
    fclose(fp);
    return WA_CONFIG_ACCEPT;
WA_intersection_table_error:
    LOG_MSG_ERROR("WA intersection table config read fail.\n");
    LOG_MSG_TRACE("WA intersection table config read fail.\n");
    return -1;
}
