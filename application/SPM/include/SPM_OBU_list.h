#ifndef SPM_OBU_LIST_H
#define SPM_OBU_LIST_H

#include "typedefine.h"
#include <pthread.h>

typedef struct SPM_OBU_obj {
    int OBU_id;
    char OBU_name[OBU_NAME_MAX_LEN + 1];  //+1 if for \0
    uint8_t vehicle_type;
    struct SPM_OBU_obj *next;
}SPM_OBU_obj_t;

extern SPM_OBU_obj_t *SPM_OBU_obj_head;
extern pthread_mutex_t SPM_OBU_obj_mutex;

void SPM_OBU_obj_insert(OBU_object_t *OBU_obj);
#endif