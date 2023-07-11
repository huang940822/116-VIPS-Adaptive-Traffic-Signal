#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

#include "j2735_data_component.h"
#include "typedefine.h"
#include "vector.h"

#define MAP_CONFIG_FILE FILE_PATH "application/MAP/config/config.txt"

#define LANE_MAX_NUMBER 5
#define DIRECTION_MAX_NUMBER 8

int MAP_config_init();

typedef struct MAP_Node {
    double lon;
    double lat;
} MAP_Node_t;

typedef struct MAP_config_lane {
    int32_t laneID;
    LaneDirection direction;
    uint8_t approach;
    uint8_t lane_index;
    vector_t(MAP_Node_t) node_list;
} MAP_config_lane_t;

typedef struct MAP_config_connectsTo {
    uint8_t config_laneID;
    vector_t(uint8_t) left_laneId;
    vector_t(uint8_t) stright_laneId;
    vector_t(uint8_t) right_laneId;
} MAP_config_connectsTo_t;

/* Return codes of config */
typedef enum MAP_config_err {
    MAP_CONFIG_ACCEPT = 0,
    MAP_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED = -1,
    MAP_CONFIG_INVALID = -2,
    MAP_CONFIG_INVALID_OPEN_FILE = -3,
} MAP_config_err_t;

typedef struct MAP_config_object {
    vector_t(MAP_config_lane_t) lane_list;
    vector_t(MAP_config_connectsTo_t) connectsTo_list;
    uint8_t MAP_packet_transfer_speed;
    uint8_t MAP_dontSend2TC;
} MAP_config_object_t;

void print_config_map(MapData *map, char *buf, int buf_len);

extern MAP_config_object_t MAP_config;

#endif