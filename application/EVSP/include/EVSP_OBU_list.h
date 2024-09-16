#ifndef EVSP_OBU_LIST_H
#define EVSP_OBU_LIST_H

#include <pthread.h>

#include "EVSP_typedefine.h"
#include "list.h"

typedef struct EVSP_OBU_update_info {
    float lat;
    float lon;
    float speed;
    uint8_t direction;
    uint8_t is_touching; // 目前在觸發領域裡
    uint8_t is_activate; // 目前觸發狀態 (0: 未觸發; 1: pre-activate (進入觸發領域，觸碰次數未超過閾值); 2: 已觸發)
} EVSP_OBU_update_info_t;

typedef struct EVSP_cooling_info {
    char OBU_name[OBU_NAME_MAX_LEN];
    uint32_t touching_area_id;
    time_t terminate_time;
    struct list_head node;
} EVSP_cooling_info_t;

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_new(char *OBU_name,
                                           uint8_t target_phase,
                                           EVSP_touching_area_t *area_ptr);
EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_insert(char *OBU_name,
                                              uint8_t target_phase,
                                              EVSP_touching_area_t *area_ptr,
                                              EVSP_OBU_update_info_t *info);
EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_search(char *OBU_name, EVSP_OBU_update_info_t *info);
void EVSP_host_OBU_obj_delete(char *OBU_name);
void EVSP_host_OBU_obj_print();
bool EVSP_host_OBU_obj_resume(uint8_t target_phase);

void EVSP_cooling_list_insert(char *OBU_name, EVSP_touching_area_t *area_ptr);
int EVSP_cooling_list_search(char *OBU_name, EVSP_touching_area_t *area_ptr);

#endif