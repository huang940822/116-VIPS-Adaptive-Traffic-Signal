#ifndef EVSP_CONFIG_H
#define EVSP_CONFIG_H

#include "typedefine.h"

#define EVSP_CONFIG_FILE "./application/EVSP/config/config.txt"
#define CONFIG_LINE_BUFFER_SIZE 100
#define MAX_CONFIG_VARIABLE_LEN 100

extern EVSP_config_object_t EVSP_config;

int EVSP_config_init();

/* Return codes of config */
typedef enum EVSP_config_err {
    EVSP_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_EVSP_HOST_OBU_PACKET_TIMEOUT = -1,
    CONFIG_INVALID_EVSP_HOST_OBU_LIST_TIMEOUT = -2,
    CONFIG_INVALID_MIN_GREEN = -3,
    CONFIG_INVALID_MAX_GREEN = -4,
    CONFIG_INVALID_VALID_RECORD_DISTANCE = -5,
    CONFIG_INVALID_EVSP_OPEN_FILE = -6
} EVSP_config_err_t;



#endif