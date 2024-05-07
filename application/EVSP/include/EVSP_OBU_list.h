#ifndef EVSP_OBU_LIST_H
#define EVSP_OBU_LIST_H

#include <pthread.h>
#include "EVSP_typedefine.h"

typedef struct EVSP_OBU_update_info {
    float lat;
    float lon;
    float speed;
    uint8_t direction;
} EVSP_OBU_update_info_t;

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

#endif