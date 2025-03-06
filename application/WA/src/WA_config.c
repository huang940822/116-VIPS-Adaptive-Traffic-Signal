#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WA_config.h"
#include "config.h"
#include "log.h"

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
        log_file_write_fatal_error("error opening %s", WA_CONFIG_FILE);
        return CONFIG_INVALID_WA_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", WA_CONFIG_FILE);
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
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.warning_freq = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_WARNING_FREQ;
                }
            } else {
                return CONFIG_INVALID_WA_WARNING_FREQ;
            }
        }

        // BRANCH_TO_MAIN
        if (strstr(read_buf, "BRANCH_TO_MAIN")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.branch2main = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_BRANCH2MAIN;
                }
            } else {
                return CONFIG_INVALID_WA_BRANCH2MAIN;
            }
        }
        // BRANCH_TO_MAIN_WARNING_RANGE
        if (strstr(read_buf, "BRANCH_2_MAIN_WARNING_RANGE")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (val >= 0) {
                    WA_config.branch2mainRange = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_BRANCH2MAINWARNINGRANGE;
                }
            } else {
                return CONFIG_INVALID_WA_BRANCH2MAINWARNINGRANGE;
            }
        }
        // MAIN_DIRECTION
        if (strstr(read_buf, "MAIN_DIRECTION")) {
            if (read_int_from_config_line(read_buf, &val)) {
                if (float_val >= 0) {
                    WA_config.maindirection = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_MAINDIRECTION;
                }
            } else {
                return CONFIG_INVALID_WA_BRANCH2MAINWARNINGRANGE;
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
        
        
        // INTERSECTION_COUNT
        if (strstr(read_buf, "INTERSECTION_COUNT")) {
            if (read_int_from_config_line(read_buf, &val)){
                if (val >= 0){
                    WA_config.traffic_light.intersection_count = val;
                    continue;
                } else {
                    return CONFIG_INVALID_WA_PACKET;
                }
            } else {
                return CONFIG_INVALID_WA_PACKET;
            }
        }
        // intersection table
        if (strstr(read_buf, "intersection_table")) {
            while (true) {
                if (feof(fp))
                    goto WA_intersection_table_error;
                char *buf = read_line(read_buf, sizeof(read_buf), fp);
                if (strstr(buf, "intersection_table_end"))
                    break;
                if (buf == NULL)
                    continue;
                
                uint8_t direction = 0;
                double lat;
                double lon;
                char *sepstr = buf;
                char *substr = trim_comments(strsep(&sepstr, ","));
                
                /* direction */
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &direction) != 1)
                    goto WA_intersection_table_error;

                /* lat and lon */
                substr = trim_comments(strsep(&sepstr, ","));

                if(substr == NULL || sscanf(substr, "%lf %lf", &lat, &lon) != 2)
                    goto WA_intersection_table_error;
                
                WA_config.traffic_light.tab[direction].Traffic_light_lat = lat;
                WA_config.traffic_light.tab[direction].Traffic_light_lon = lon;
                
            }
            log_file_write("Warning Frequecy: %d", WA_config.warning_freq);
            for (int i = 0; i < WA_config.traffic_light.intersection_count; i++){
                    log_file_write("direction:%hhd, lat:%lf, lon:%lf\n", i, 
                                                                        WA_config.traffic_light.tab[i].Traffic_light_lat, 
                                                                        WA_config.traffic_light.tab[i].Traffic_light_lon);
            }
            log_file_write("Branch2main: %d, Branch2mainRange: %d, maindirection: %d\n", WA_config.branch2main, WA_config.branch2mainRange, WA_config.maindirection);
        }
    }
    fclose(fp);
    return WA_CONFIG_ACCEPT;
WA_intersection_table_error:
    log_file_write_fatal_error("WA intersection table config read fail.\n");
    printf("WA intersection table config read fail.\n");
    return -1;
}
