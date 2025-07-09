#ifndef TIB_CONFIG_H
#define TIB_CONFIG_H

#include "TIB_utils.h"
#include "j2735_data_component.h"
#include "list.h"
#include "typedefine.h"
#include "vector.h"

#define TIB_CONFIG_DIR FILE_PATH "application/TIB/config/"
#define TIB_CONFIG_FILENAME "_config.txt"

#define LANE_MAX_NUMBER 5
#define DIRECTION_MAX_NUMBER 8
#define TIB_TABLE_DELIM ","
#define TIB_FIELD_DELIM " "

int TIB_config_init();

/* Return codes of config */
typedef enum TIB_config_err {
    TIB_CONFIG_ACCEPT = 0,
    TIB_CONFIG_INVALID_OPEN_FILE = -1,
    TIB_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED = -2,
    TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED = -3,
    TIB_CONFIG_LaneSet_table_INVALID = -4,
    TIB_CONFIG_LaneSet_ConnectsTo_table_INVALID = -5,
    TIB_CONFIG_SignalGroupID_table_INVALID = -6,
    TIB_CONFIG_TIM_table_INVALID = -7,
    TIB_CONFIG_RSA_table_INVALID = -8,
    TIB_CONFIG_INVALID_EVA_max_distance = -9,
    TIB_CONFIG_INVALID_EVA_touching_threshold = -9,
    TIB_CONFIG_INVALID_EVA_touching_area_config_type = -10,
    TIB_CONFIG_INVALID_EVA_cooling_time = -11,
    TIB_CONFIG_INVALID_GENERAL_PACKET_TIME_PER_CYCLE = -12,
    TIB_CONFIG_INVALID_MAP_PACKET_CYCLE_PER_TRANSFER = -13,
    TIB_CONFIG_INVALID_SPAT_PACKET_CYCLE_PER_TRANSFER = -14,
    TIB_CONFIG_INVALID_TIM_PACKET_CYCLE_PER_TRANSFER = -15,
    TIB_CONFIG_INVALID_RSA_PACKET_CYCLE_PER_TRANSFER = -16,
    TIB_CONFIG_INVALID_PSM_PACKET_CYCLE_PER_TRANSFER = -17,
    TIB_CONFIG_INVALID_EVA_PACKET_CYCLE_PER_TRANSFER = -18,    
} TIB_config_err_t;

typedef struct MAP_Node {
    double lon;
    double lat;
} MAP_Node_t;

typedef struct TIM_Position_Node {
    double lon;
    double lat;
} TIM_Position_Node_t;

typedef struct TIM_Path_Node {
    double lon;
    double lat;
} TIM_Path_Node_t;

typedef struct RSA_Position_Node {
    double lon;
    double lat;
} RSA_Position_Node_t;

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

typedef struct signalID_obj {
    uint8_t signalGroupID;
    uint8_t approachId;
    uint8_t signalGreenType;;
} signalID_obj_t;

typedef enum TIM_directionality {
    dir_unavailable=0,
    dir_forward=1,
} TIM_directionality_t;

typedef struct TIM_config_sign {
    uint8_t TimMsgID;
    uint8_t FrameType;
    vector_t(TIM_Position_Node_t) TimPosition;    
    uint8_t viewAngle[2];
    float anchor[2];
    uint8_t directionality;
    uint8_t BroadcastDirection[2];
    uint8_t node_count;
    vector_t(TIM_Path_Node_t) TimPath;
    uint8_t eventType;
    uint16_t EventDescription[8];
} TIM_config_sign_t;

typedef struct RSA_config_event {
    uint8_t RoadMsgID;
    uint8_t priority;
    vector_t(RSA_Position_Node_t) RsaPosition;
    uint8_t heading[2];
    uint8_t extent;
    uint8_t eventType;
    uint16_t EventDescription[8];
} RSA_config_event_t;

typedef struct TIB_config_object {
    uint8_t TIB_dontSend2TC;
    uint8_t MAP_packet_transfer_speed;
    uint8_t SPaT_packet_transfer_speed;
    uint8_t EVA_packet_transfer_speed;
    uint8_t MAP_packet_cycle_per_transfer;
    uint8_t SPaT_packet_cycle_per_transfer;
    uint8_t TIM_packet_cycle_per_transfer;
    uint8_t RSA_packet_cycle_per_transfer;
    uint8_t PSM_packet_cycle_per_transfer;
    uint8_t EVA_packet_cycle_per_transfer;
    float general_packet_time_per_cycle;
    vector_t(MAP_config_lane_t) lane_list;
    vector_t(MAP_config_connectsTo_t) connectsTo_list;
    // 用號控器上的第機車道區分 approachID
    // 只有是車道 並且是 ingress
    struct list_head MAP_lane_approach[COMPASS_NUM];
    // 人行道的方向
    struct list_head MAP_sidewalk_approach[COMPASS_NUM];

    // 對應號控器上哪一路有哪些綠燈 signalGroupId 是多少
    // 給 MAP 看得
    int16_t signalGroupId_table[COMPASS_NUM][NumOfGreen];
    // 避免 ApproachID 與 號控器內的 SignalID 不同
    // 給 SPaT 看得
    vector_t(signalID_obj_t) signalId_table[COMPASS_NUM][NumOfGreen];
    // 對應某一路口有哪些號誌
    // 給 TIM 看
    vector_t(TIM_config_sign_t) TIM_table;
} TIB_config_object_t;

void print_config_map(MapData *map, char *buf, int buf_len);
void print_config_tim(TravelerInformation *tim, char *buf, int buf_len);
extern TIB_config_object_t TIB_config;

#endif