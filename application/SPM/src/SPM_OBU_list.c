#include "SPM_OBU_list.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "error_status.h"
#include "log.h"
#include "string.h"

SPM_OBU_obj_t *SPM_OBU_obj_head = NULL;
pthread_mutex_t SPM_OBU_obj_mutex = PTHREAD_MUTEX_INITIALIZER;

SPM_OBU_obj_t *SPM_OBU_obj_new(OBU_object_t *OBU_obj)
{
    SPM_OBU_obj_t *SPM_OBU_obj;
    Malloc(SPM_OBU_obj, sizeof(SPM_OBU_obj_t), "SPM_OBU_obj_new");

    strncpy(SPM_OBU_obj->OBU_name, OBU_obj->OBU_name, OBU_NAME_MAX_LEN);
    return SPM_OBU_obj;
}

void SPM_OBU_obj_insert(OBU_object_t *OBU_obj, SignalRequestMessage *p_srm)
{
    pthread_mutex_lock(&SPM_OBU_obj_mutex);
    SPM_OBU_obj_t *current = SPM_OBU_obj_head;
    if (SPM_OBU_obj_head == NULL) {
        current = SPM_OBU_obj_head = SPM_OBU_obj_new(OBU_obj);
    } else {
        SPM_OBU_obj_t *previous = current;
        while (current) {
            if (strcmp(current->OBU_name, OBU_obj->OBU_name) == 0) {
                goto SPM_OBU_obj_insert_end;
            }
            previous = current;
            current = current->next;
        }
        current = previous->next = SPM_OBU_obj_new(OBU_obj);
    }
    current->id.choice = p_srm->requestor.id.choice;
    if (current->id.choice == VehicleID_entityID)
        strncat(current->id.u.buf, p_srm->requestor.id.u.entityID.buf, 4);
    else
        current->id.u.stationID = p_srm->requestor.id.u.stationID;
    current->role = p_srm->requestor.type.role;
SPM_OBU_obj_insert_end:
    current->time_second = OBU_obj->record_ring.record[OBU_obj->record_ring.last_record_pointer].time_second;
    current->sigRequest_count = 0;
    for (int i = 0; i < p_srm->requests.count; i++) {
        /* Only record for the specific intersection, filter the request by intersection id. */
        if (config.RSU_id == p_srm->requests.tab[i].request.id.id) {
            if (p_srm->requests.tab[i].request.id.region_option && p_srm->requests.tab[i].request.id.region != config.RSU_region)
                continue;
            memcpy(&current->sigRequestList[current->sigRequest_count], &p_srm->requests.tab[i], sizeof(SignalRequestPackage));
            current->sigRequest_count += 1;
        }
    }
    pthread_mutex_unlock(&SPM_OBU_obj_mutex);
}

void SPM_OBU_obj_delete(char *OBU_name)
{
    pthread_mutex_lock(&SPM_OBU_obj_mutex);
    SPM_OBU_obj_t *current = SPM_OBU_obj_head, *previous = SPM_OBU_obj_head;
    while (current) {
        if (strcmp(current->OBU_name, OBU_name) == 0) {
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