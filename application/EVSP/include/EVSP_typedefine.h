#ifndef EVSP_TYPEDEFINE_H
#define EVSP_TYPEDEFINE_H

#include <stdbool.h>
#include <sys/types.h>
#include "typedefine.h"

#define EVSP_PHASE_MAX 8
#define MIN_GREEN 5
#define MAX_GREEN 120
#define VALID_RECORD_DISTANCE 5
#define MAX_NODE_COUNT 100

typedef struct EVSP_Node {
    union {
        double lon;
        double x;
    };
    union {
        double lat;
        double y;
    };
} EVSP_Node_t;

// 因為只有把 EVSP_Node 合併成 line 所以使用指標
typedef struct EVSP_Line {
    EVSP_Node_t *p1;
    EVSP_Node_t *p2;
} EVSP_Line_t;

typedef struct EVSP_static_space {
    uint8_t on_duty_flag;
    uint8_t weight;
    uint8_t error_code;
    uint8_t last_direction;
    float last_lon;
    float last_lat;
} EVSP_static_space_t;

typedef struct EVSP_terminate_area {
    uint32_t terminate_area_id;

    uint8_t node_count;
    EVSP_Node_t *node;
} EVSP_terminate_area_t;

typedef struct EVSP_touching_area {
    uint32_t touching_area_id;

    uint8_t direciton_start;  // 表達一段範圍的方向 總共 0~7 為順時針 6~0 是 6, 7, 0 的意思
    uint8_t direciton_end;    // 從 start 到 end 的範圍都算如果只有一個就是 start == end
    uint8_t node_count;
    uint8_t terminate_area_count;

    EVSP_Node_t *node;
    uint32_t *terminate_area_Id;
} EVSP_touching_area_t;

typedef struct EVSP_plan_subPhase {
    uint8_t SubPhaseID;

    uint8_t touching_area_count;
    uint32_t *touching_area_Id;
} EVSP_plan_subPhase_t;

typedef struct EVSP_plan_table {
    uint8_t plan_id_count;
    uint8_t plan_id_max;
    uint8_t plan_subPhase_count;
    uint8_t plan_subPhase_max;

    uint8_t *plan_id;  // 因為可能很多個 plan 都是用同一個 plan table
    EVSP_plan_subPhase_t *plan_subPhase;
} EVSP_plan_table_t;

typedef struct EVSP_plan_list_t {
    uint8_t terminate_area_count;
    uint8_t terminate_area_max;
    uint8_t touching_area_count;
    uint8_t touching_area_max;
    uint8_t plan_table_count;
    uint8_t plan_table_max;

    EVSP_terminate_area_t *terminate_area;
    EVSP_touching_area_t *touching_area;
    EVSP_plan_table_t *plan_table;
} EVSP_plan_list_t;

typedef struct EVSP_host_OBU_obj {
    char OBU_name[ID_MAX_LEN + 1];
    uint8_t passenger;
    uint8_t target_phase;
    uint16_t distance;
    timer_t host_OBU_packet_timer;
    timer_t host_OBU_list_timer;
    struct EVSP_touching_area *area_ptr;
    struct EVSP_host_OBU_obj *next;
    float lon;
    float lat;
    float speed;
    int direction;
    uint8_t touched_amount; // 觸碰點觸碰次數
    int is_activate; // 已觸發狀態 (0: 未觸發; 1: pre-activate (進入觸發領域，觸碰次數未超過閾值); 2: 已觸發)
    vehicle_type_t vehicle_type;
} EVSP_host_OBU_obj_t;

#endif