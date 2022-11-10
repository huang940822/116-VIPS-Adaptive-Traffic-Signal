#include "SPM_OBU_list.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "error_status.h"
#include "log.h"
#include "string.h"

SPM_OBU_obj_t *SPM_OBU_obj_head = NULL;
pthread_mutex_t SPM_OBU_obj_mutex = PTHREAD_MUTEX_INITIALIZER;

SPM_OBU_obj_t *SPM_OBU_obj_new(OBU_object_t *OBU_obj)
{
    SPM_OBU_obj_t *SPM_OBU_obj;
    Malloc(SPM_OBU_obj, sizeof(SPM_OBU_obj_t), "SPM_OBU_obj_new");
    SPM_OBU_obj->OBU_id = OBU_obj->OBU_id;
    strcpy(SPM_OBU_obj->OBU_name, OBU_obj->OBU_name);
    return SPM_OBU_obj;
}

void SPM_OBU_obj_insert(OBU_object_t *OBU_obj)
{
    pthread_mutex_lock(&SPM_OBU_obj_mutex);
    if (SPM_OBU_obj_head == NULL) {
        SPM_OBU_obj_head = SPM_OBU_obj_new(OBU_obj);
    } else {
        SPM_OBU_obj_t *current = SPM_OBU_obj_head, *previous = SPM_OBU_obj_head;
        while (current) {
            if (OBU_obj->OBU_id == current->OBU_id) {
                pthread_mutex_unlock(&SPM_OBU_obj_mutex);
                return;
            }
            previous = current;
            current = current->next;
        }
        previous->next = SPM_OBU_obj_new(OBU_obj);
    }
    pthread_mutex_unlock(&SPM_OBU_obj_mutex);
}

void SPM_OBU_obj_delete(int OBU_id)
{
    pthread_mutex_lock(&SPM_OBU_obj_mutex);
    SPM_OBU_obj_t *current = SPM_OBU_obj_head, *previous = SPM_OBU_obj_head;
    while (current) {
        if (current->OBU_id == OBU_id) {
            if (current == SPM_OBU_obj_head) {
                SPM_OBU_obj_head = NULL;
            } else {
                previous->next = current->next;
            }
            free(current);
            break;
        }
    }
    pthread_mutex_unlock(&SPM_OBU_obj_mutex);
}