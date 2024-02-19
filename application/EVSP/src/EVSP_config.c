#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "EVSP_config.h"
#include "config.h"
#include "log.h"
#include "typedefine.h"

// EVSP_config_object_t EVSP_config;

EVSP_config_object_t EVSP_config = {
    .evsp_host_obu_packet_timeout = 20,
    .evsp_host_obu_list_timeout = 120,
    .min_green = 5,
    .max_green = 120,
    .valid_record_distance = 5,
    .touching_area_config_type = EVSP_touching_area_DEFAULT,
};

int EVSP_config_init()
{
    FILE *fp;
    fp = fopen(EVSP_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", EVSP_CONFIG_FILE);
        return CONFIG_INVALID_EVSP_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", EVSP_CONFIG_FILE);
    }

    char buf[CONFIG_LINE_BUFFER_SIZE];

    uint8_t uint8_t_val;
    float float_val;
    char string_val[MAX_CONFIG_VARIABLE_LEN];

    while (!feof(fp)) {
        fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
            continue;
        }


        // evsp_host_obu_packet_timeout
        if (strstr(buf, "evsp_host_obu_packet_timeout ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP_config.evsp_host_obu_packet_timeout = uint8_t_val;
                    log_file_write("config: evsp_host_obu_packet_timeout = %d",
                                   EVSP_config.evsp_host_obu_packet_timeout);
                    continue;
                } else {
                    return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
            }
        }

        // evsp_host_obu_list_timeout
        if (strstr(buf, "evsp_host_obu_list_timeout ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP_config.evsp_host_obu_list_timeout = uint8_t_val;
                    log_file_write("config: evsp_host_obu_list_timeout = %d",
                                   EVSP_config.evsp_host_obu_list_timeout);
                    continue;
                } else {
                    return CONFIG_INVALID_EVSP_HOST_OBU_LIST_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_LIST_TIMEOUT;
            }
        }

        // min_green
        if (strstr(buf, "min_green ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP_config.min_green = uint8_t_val;
                    log_file_write("config: min_green = %d", EVSP_config.min_green);
                    continue;
                } else {
                    return CONFIG_INVALID_MIN_GREEN;
                }
            } else {
                return CONFIG_INVALID_MIN_GREEN;
            }
        }

        // evsp_host_obu_packet_timeout
        if (strstr(buf, "max_green ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP_config.max_green = uint8_t_val;
                    log_file_write("config: max_green = %d", EVSP_config.max_green);
                    continue;
                } else {
                    return CONFIG_INVALID_MAX_GREEN;
                }
            } else {
                return CONFIG_INVALID_MAX_GREEN;
            }
        }

        // evsp_host_obu_packet_timeout
        if (strstr(buf, "valid_record_distance ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP_config.valid_record_distance = uint8_t_val;
                    log_file_write("config: valid_record_distance = %d",
                                   EVSP_config.valid_record_distance);
                    continue;
                } else {
                    return CONFIG_INVALID_VALID_RECORD_DISTANCE;
                }
            } else {
                return CONFIG_INVALID_VALID_RECORD_DISTANCE;
            }
        }

        // touching_area_config
        if (strstr(buf, "touching_area_config_type ")) {
            char val[MAX_CONFIG_VARIABLE_LEN];
            if (read_string_from_config_line(buf, val)) {
                if (strncmp(val, "default", sizeof("default") - 1) == 0) {
                    EVSP_config.touching_area_config_type = EVSP_touching_area_DEFAULT;
                } else if (strncmp(val, "table", sizeof("table") - 1) == 0) {
                    EVSP_config.touching_area_config_type = EVSP_touching_area_TABLE;
                } else {
                    return CONFIG_INVALID_OTHER;
                }
                log_file_write("config: touching_area_config_type = %s", val);
                continue;
            } else {
                return CONFIG_INVALID_OTHER;
            }
        }

        // dontSend2TC
        if (strstr(buf, "dontSend2TC ")) {
            char val[MAX_CONFIG_VARIABLE_LEN];
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    EVSP.dontSend2TC = uint8_t_val;
                    log_file_write("config: dontSend2TC = %d", EVSP.dontSend2TC);
                    continue;
                } else {
                    return CONFIG_INVALID_OTHER;
                }
            } else {
                return CONFIG_INVALID_OTHER;
            }
        }
    }

    fclose(fp);
    return EVSP_CONFIG_ACCEPT;
}