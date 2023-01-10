#ifndef EVSP_CONFIG_H
#define EVSP_CONFIG_H

#include "typedefine.h"

#define EVSP_CONFIG_FILE FILE_PATH "application/EVSP/config/config.txt"

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

typedef enum EVSP_touching_area_config_type {
    EVSP_touching_area_DEFAULT = 0, // 原本的格式
    EVSP_touching_area_TABLE = 1, // 新的格式
} EVSP_touching_area_config_type_t;

typedef struct EVSP_config_object {
    uint8_t evsp_host_obu_packet_timeout;
    uint8_t evsp_host_obu_list_timeout;
    uint8_t min_green;
    uint8_t max_green;
    uint8_t valid_record_distance;
    EVSP_touching_area_config_type_t touching_area_config_type;

} EVSP_config_object_t;

extern EVSP_config_object_t EVSP_config;

#endif