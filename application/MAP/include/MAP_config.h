#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

#include "typedefine.h"

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

typedef struct LaneID_connectingLane {
    uint8_t LaneID;
    uint8_t LeftconnectingLane[LANE_MAX_NUMBER];
    uint8_t StrightconnectingLane[LANE_MAX_NUMBER];
    uint8_t RightconnectingLane[LANE_MAX_NUMBER];
} LaneID_connectingLane_t;

typedef struct Direction_lane_ {
    uint8_t Lane_count;
    LaneID_connectingLane_t connectingLane[LANE_MAX_NUMBER];
} Direction_lane_t;

typedef struct Direction_total_information {
    Direction_lane_t Direction[DIRECTION_MAX_NUMBER];
}Direction_total_information_t;

typedef struct MAP_config_object {
    MapData *Mapconfig;
    uint8_t MAP_packet_transfer_speed;
    Direction_total_information_t map_lane2connecting;
    uint8_t MAP_dontSend2TC;
} MAP_config_object_t;

extern MAP_config_object_t MAP_config;

void print_config_map(char *buf, int buf_len);
#endif