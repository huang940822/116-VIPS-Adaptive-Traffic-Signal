#ifndef EVSP_TYPEDEFINE_H
#define EVSP_TYPEDEFINE_H

#include "typedefine.h"

#define EVSP_PHASE_MAX 8
#define MIN_GREEN 5
#define MAX_GREEN 120
#define VALID_RECORD_DISTANCE 5

typedef struct EVSP_static_space {
    uint8_t on_duty_flag;
    uint8_t weight;
    uint8_t error_code;
    uint8_t last_direction;
    float last_lon;
    float last_lat;
} EVSP_static_space_t;

typedef struct EVSP_touching_area {
    uint8_t direciton;
    float lon_high;
    float lon_low;
    float lat_high;
    float lat_low;
    struct EVSP_touching_area *terminate;
    struct EVSP_touching_area *next;
} EVSP_touching_area_t;

typedef struct EVSP_touching_area_plan_list {
    uint8_t plan_id;
    EVSP_touching_area_t list[EVSP_PHASE_MAX];
    struct EVSP_touching_area_plan_list *next;
} EVSP_touching_area_plan_list_t;


typedef struct EVSP_host_OBU_obj {
    char OBU_name[OBU_NAME_MAX_LEN + 1];
    uint8_t passenger;
    uint8_t target_phase;
    uint16_t distance;
    timer_t host_OBU_packet_timer;
    timer_t host_OBU_list_timer;
    struct EVSP_touching_area *area_ptr;
    struct EVSP_host_OBU_obj *next;
} EVSP_host_OBU_obj_t;

#endif