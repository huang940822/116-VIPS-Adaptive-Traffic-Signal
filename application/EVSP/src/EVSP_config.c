#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "EVSP_config.h"
#include "log.h"
#include "typedefine.h"

// EVSP_config_object_t EVSP_config;

EVSP_config_object_t EVSP_config = {
    .evsp_host_obu_packet_timeout = 20,
    .evsp_host_obu_list_timeout = 120,
    .min_green = 5,
    .max_green = 120,
    .valid_record_distance = 5,
};

static bool read_uint8_t_from_config_line(char *config_line, uint8_t *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %hhd\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

int EVSP_config_init()
{
    FILE *fp;
    fp = fopen(EVSP_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", EVSP_CONFIG_FILE);
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
                    return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
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
                    return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
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
                    return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
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
                    return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
            }
        }
    }

    fclose(fp);
    return EVSP_CONFIG_ACCEPT;
}