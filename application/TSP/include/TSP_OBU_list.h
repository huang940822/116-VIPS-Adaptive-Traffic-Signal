#ifndef TSP_OBU_LIST_H
#define TSP_OBU_LIST_H

#include "TSP_typedefine.h"

extern TSP_host_OBU_obj_t TSP_host_OBU_list;
extern pthread_mutex_t TSP_host_OBU_list_mutex;

TSP_host_OBU_obj_t *TSP_host_OBU_obj_new(char *OBU_id, uint8_t target_phase);
TSP_host_OBU_obj_t *TSP_host_OBU_obj_insert(char *OBU_id, uint8_t target_phase);
TSP_host_OBU_obj_t *TSP_host_OBU_obj_search(char *OBU_id);
void TSP_host_OBU_obj_delete(char *OBU_id);
void TSP_host_OBU_obj_print();
void TSP_host_OBU_obj_first_insert(char *OBU_id, uint8_t target_phase);

#endif