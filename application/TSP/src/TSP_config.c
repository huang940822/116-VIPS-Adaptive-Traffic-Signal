#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "TSP_config.h"
#include "TSP_typedefine.h"
#include "TSP.h"

// EVSP_config_object_t EVSP_config;

TSP_config_object_t TSP_config = {
    .tsp_host_obu_list_timeout=120,
    .tsp_remaining_distance_max=500,
};

static bool read_uint8_t_from_config_line(char* config_line, uint8_t *val) {    
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %hhd\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

static bool read_uint16_t_from_config_line(char* config_line, uint16_t *val) {    
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %hu\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

int TSP_config_init()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    FILE *fp;
    fp = fopen(TSP_CONFIG_FILE, "r");
    if(fp == NULL) {						
        log_file_write_fatal_error("error opening %s", TSP_CONFIG_FILE);
	} else {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s opened successfully", TSP_CONFIG_FILE);
        log_file_write(log_content);
	}

    char buf[CONFIG_LINE_BUFFER_SIZE];

    uint8_t uint8_t_val;
    uint16_t uint16_t_val;
    float float_val;
    char string_val[MAX_CONFIG_VARIABLE_LEN];

    while (!feof(fp)) {
        memset(log_content, 0, sizeof(log_content));

        fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
            continue;
        }


        // TSP_config_object_t TSP_config = {
        //     .tsp_host_obu_list_timeout=120,
        //     .tsp_remaining_distance_max=500,
        // };

        //evsp_host_obu_packet_timeout
        if (strstr(buf, "tsp_host_obu_list_timeout ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TSP_config.tsp_host_obu_list_timeout = uint8_t_val;
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "config: tsp_host_obu_list_timeout = %d", TSP_config.tsp_host_obu_list_timeout);
                    log_file_write(log_content);
                    continue;
                } else {
                    return -1;//CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return -1;//CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
            }
        }

        //evsp_host_obu_list_timeout
        if (strstr(buf, "tsp_remaining_distance_max ")) {
            if (read_uint16_t_from_config_line(buf, &uint16_t_val)) {
                if (uint16_t_val >= 0) {
                    TSP_config.tsp_remaining_distance_max = uint16_t_val;
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "config: tsp_remaining_distance_max = %d", TSP_config.tsp_remaining_distance_max);
                    log_file_write(log_content);
                    continue;
                } else {
                    return -1;//CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
                }
            } else {
                return -1;//CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT;
            }
        }

        
        
    }

    fclose(fp);
    return 0;//EVSP_CONFIG_ACCEPT;
}