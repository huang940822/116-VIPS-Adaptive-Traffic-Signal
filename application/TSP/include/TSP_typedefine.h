#ifndef TSP_TYPEDEFINE_H
#define TSP_TYPEDEFINE_H

#include "typedefine.h"

#define TSP_CYCLE_MAX 2
#define TSP_PHASE_MAX 8

#define TSP_REMAINING_DISTANCE_MAX 500
#define TSP_SIGNAL_PHASE_MAX 8
#define TSP_REMAINING_TIME_MAX 200
#define TSP_TARGET_PHASE_MAX 8

#define TSP_REMAINING_DISTANCE_INTERVAL 10
#define TSP_SIGNAL_PHASE_INTERVAL 1
#define TSP_REMAINING_TIME_INTERVAL 2
#define TSP_TARGET_PHASE_INTERVAL 1

#define TSP_REMAINING_DISTANCE_NUM \
    TSP_REMAINING_DISTANCE_MAX / TSP_REMAINING_DISTANCE_INTERVAL
#define TSP_SIGNAL_PHASE_NUM TSP_SIGNAL_PHASE_MAX / TSP_SIGNAL_PHASE_INTERVAL
#define TSP_REMAINING_TIME_NUM \
    TSP_REMAINING_TIME_MAX / TSP_REMAINING_TIME_INTERVAL
#define TSP_TARGET_PHASE_NUM TSP_TARGET_PHASE_MAX / TSP_TARGET_PHASE_INTERVAL

typedef struct TSP_static_space {
    uint8_t on_duty_flag;
    uint8_t passenger_num;
} TSP_static_space_t;

typedef struct TSP_adjustment {
    int8_t adjustment[TSP_CYCLE_MAX][TSP_PHASE_MAX];
} TSP_adjustment_t;

typedef struct TSP_driving_recommend {
    uint8_t recommend_speed;
    uint8_t passing_rate;
} TSP_driving_recommend_t;

typedef struct TSP_RSU_matrix {
    uint8_t plan_id;
    TSP_adjustment_t entry[TSP_REMAINING_DISTANCE_NUM][TSP_SIGNAL_PHASE_NUM]
                          [TSP_REMAINING_TIME_NUM][TSP_TARGET_PHASE_NUM];
    struct TSP_RSU_matrix *next;
} TSP_RSU_matrix_t;

typedef struct TSP_OBU_matrix {
    uint8_t plan_id;
    TSP_driving_recommend_t entry[TSP_REMAINING_DISTANCE_NUM]
                                 [TSP_SIGNAL_PHASE_NUM][TSP_REMAINING_TIME_NUM]
                                 [TSP_TARGET_PHASE_NUM];
    struct TSP_OBU_matrix *next;
} TSP_OBU_matrix_t;

typedef struct TSP_host_OBU_obj {
    char OBU_name[OBU_NAME_MAX_LEN];
    uint8_t passenger;
    uint8_t target_phase;
    uint16_t distance;
    timer_t host_OBU_list_timer;
    struct TSP_host_OBU_obj *next;
} TSP_host_OBU_obj_t;

typedef struct R2V_packet {
    uint8_t control_status;
    uint8_t host_RSU_pass_probability;
    uint8_t host_RSU_speed_suggest;
} R2V_packet_t;

#endif