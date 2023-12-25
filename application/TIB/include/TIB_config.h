#ifndef TIB_CONFIG_H
#define TIB_CONFIG_H

#include "TIB_utils.h"
#include "j2735_data_component.h"
#include "list.h"
#include "typedefine.h"
#include "vector.h"

#define TIB_CONFIG_FILE FILE_PATH "application/TIB/config/config.txt"

#define LANE_MAX_NUMBER 5
#define DIRECTION_MAX_NUMBER 8
#define TIB_TABLE_DELIM ","
#define TIB_FIELD_DELIM " "

int TIB_config_init();

typedef struct MAP_Node {
    double lon;
    double lat;
} MAP_Node_t;

typedef struct MAP_config_lane {
    struct list_head approach_node;
    vector_t(MAP_Node_t) node_list;
    int32_t config_laneID;
    uint8_t direction;  // bit string 0 ingress 1 egress
    uint8_t approach;
    uint8_t lane_index;
    LaneTypeAttributes_choice lane_type;
    uint16_t lane_attributes;
    uint8_t shared_with;
} MAP_config_lane_t;

typedef struct MAP_config_connectsTo {
    uint8_t config_laneID;
    vector_t(uint8_t) left_laneId;
    vector_t(uint8_t) straight_laneId;
    vector_t(uint8_t) right_laneId;
} MAP_config_connectsTo_t;

/* Return codes of config */
typedef enum TIB_config_err {
    TIB_CONFIG_ACCEPT = 0,
    TIB_CONFIG_INVALID_OPEN_FILE = -1,
    TIB_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED = -2,
    TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED = -3,
    TIB_CONFIG_LaneSet_table_INVALID = -4,
    TIB_CONFIG_LaneSet_ConnectsTo_table_INVALID = -5,
    TIB_CONFIG_SignalGroupID_table_INVALID = -6,
} TIB_config_err_t;

typedef struct TIB_config_object {
    uint8_t TIB_dontSend2TC;
    uint8_t MAP_packet_transfer_speed;
    uint8_t SPaT_packet_transfer_speed;
    vector_t(MAP_config_lane_t) lane_list;
    vector_t(MAP_config_connectsTo_t) connectsTo_list;
    // 用號控器上的第機車道區分 approachID
    // 只有是車道 並且是 ingress
    struct list_head MAP_lane_approach[COMPASS_NUM];
    // 人行道的方向
    struct list_head MAP_sidewalk_approach[COMPASS_NUM];

    // 對應號控器上哪一路有哪些綠燈 signalGroupId 是多少
    int16_t signalGroupId_table[COMPASS_NUM][NumOfGreen];
} TIB_config_object_t;

void print_config_map(MapData *map, char *buf, int buf_len);

extern TIB_config_object_t TIB_config;

#endif