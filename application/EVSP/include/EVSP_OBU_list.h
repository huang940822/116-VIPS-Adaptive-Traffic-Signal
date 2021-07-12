#ifndef EVSP_OBU_LIST_H
#define EVSP_OBU_LIST_H

#include "EVSP_typedefine.h"

extern EVSP_host_OBU_obj_t EVSP_host_OBU_list;
extern pthread_mutex_t EVSP_host_OBU_list_mutex;

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_new(char *OBU_id, uint8_t target_phase, EVSP_touching_area_t *area_ptr);
EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_insert(char *OBU_id, uint8_t target_phase, EVSP_touching_area_t *area_ptr);
EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_search(char *OBU_id);
void EVSP_host_OBU_obj_delete(char *OBU_id);
void EVSP_host_OBU_obj_print();
bool EVSP_host_OBU_obj_resume(uint8_t target_phase);

#endif