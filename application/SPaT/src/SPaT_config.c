#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SPaT_config.h"
#include "log.h"
#include "typedefine.h"

SPaT_config_object_t SPaT_config = {
    .SPaT_packet_transfer_speed = 10,
    .signalcount = 2,
    .SPaT_dontSend2TC = 1,
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

int SPaT_config_init()
{
    FILE *fp;
    fp = fopen(SPAT_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", SPAT_CONFIG_FILE);
        return CONFIG_INVALID_SPAT_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", SPAT_CONFIG_FILE);
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
        // SPaT_packet_transfer_speed
        if (strstr(buf, "SPaT_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    SPaT_config.SPaT_packet_transfer_speed = uint8_t_val;
                    log_file_write("config: SPaT_packet_transfer_speed = %d",
                             SPaT_config.SPaT_packet_transfer_speed);
                    continue;
                } else {
                    return CONFIG_INVALID_SPAT_PACKET;
                }
            } else {
                return CONFIG_INVALID_SPAT_PACKET;
            }
        }

        // SPaT_dontSend2TC
        if (strstr(buf, "SPaT_dontSend2TC ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    SPaT_config.SPaT_dontSend2TC = uint8_t_val;
                    log_file_write("config: SPaT_dontSend2TC = %d",
                             SPaT_config.SPaT_dontSend2TC);
                    continue;
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
        }
    }

    fclose(fp);
    return SPAT_CONFIG_ACCEPT;
}