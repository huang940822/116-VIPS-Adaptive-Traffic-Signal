#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

#include "typedefine.h"
#include "vector.h"

#define MAP_CONFIG_FILE FILE_PATH "application/MAP/config/config.txt"

#define LANE_MAX_NUMBER 5
#define DIRECTION_MAX_NUMBER 8

int MAP_config_init();

/* Return codes of config */
typedef enum MAP_config_err {
    MAP_CONFIG_ACCEPT = 0,
    MAP_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED = -1,
    MAP_CONFIG_INVALID = -2,
    MAP_CONFIG_INVALID_OPEN_FILE = -3,
} MAP_config_err_t;

typedef struct MAP_config_object {
    MapData *Mapconfig;
    uint8_t MAP_packet_transfer_speed;
    uint8_t MAP_dontSend2TC;
} MAP_config_object_t;

typedef struct MAP_config_connectsTo {
    vector_t(int) left_laneId;
    vector_t(int) stright_laneId;
    vector_t(int) right_laneId;
} MAP_config_connectsTo_t;

void print_config_map(char *buf, int buf_len);

extern MAP_config_object_t MAP_config;

#endif