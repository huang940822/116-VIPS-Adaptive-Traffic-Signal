#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SPM_config.h"
#include "log.h"
#include "typedefine.h"
#include "config.h"

SPM_config_object_t SPM_config = {
    .spm_host_obu_packet_timeout = 20,
    .SPM_packet_transfer_speed = 1,
    .SPM_dontSend2TC = 1,
};

int SPM_config_init()
{
    FILE *fp;
    fp = fopen(SPM_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", SPM_CONFIG_FILE);
        return CONFIG_INVALID_SPM_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", SPM_CONFIG_FILE);
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
        // spm_host_obu_packet_timeout
        if (strstr(buf, "spm_host_obu_packet_timeout ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    SPM_config.spm_host_obu_packet_timeout = uint8_t_val;
                    log_file_write("config: spm_host_obu_packet_timeout = %d",
                             SPM_config.spm_host_obu_packet_timeout);
                    continue;
                } else {
                    goto SPM_config_init_error;
                }
            } else {
                goto SPM_config_init_error;
            }
        }
        // SPM_packet_transfer_speed
        if (strstr(buf, "SPM_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    SPM_config.SPM_packet_transfer_speed = uint8_t_val;
                    log_file_write("config: SPM_packet_transfer_speed = %d",
                             SPM_config.SPM_packet_transfer_speed);
                    continue;
                } else {
                    goto SPM_config_init_error;
                }
            } else {
                goto SPM_config_init_error;
            }
        }
        // SPM_dontSend2TC
        if (strstr(buf, "SPM_dontSend2TC ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    SPM_config.SPM_dontSend2TC = uint8_t_val;
                    log_file_write("config: SPM_dontSend2TC = %d",
                             SPM_config.SPM_dontSend2TC);
                    continue;
                } else {
                    goto SPM_config_init_error;
                }
            } else {
                goto SPM_config_init_error;
            }
        }
    }

    fclose(fp);
    return SPM_CONFIG_ACCEPT;
SPM_config_init_error:
    fclose(fp);
    return  CONFIG_INVALID_SPM_PACKET;
}