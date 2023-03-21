#include "SPM_OBU_list.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "error_status.h"
#include "log.h"
#include "string.h"

LIST_HEAD(SPM_OBU_list_head);
pthread_mutex_t SPM_OBU_obj_mutex = PTHREAD_MUTEX_INITIALIZER;

SPM_OBU_obj_t *SPM_OBU_obj_new(OBU_object_t *OBU_obj)
{
    SPM_OBU_obj_t *SPM_OBU_obj;
    Malloc(SPM_OBU_obj, sizeof(SPM_OBU_obj_t), "SPM_OBU_obj_new");

    strncpy(SPM_OBU_obj->OBU_name, OBU_obj->OBU_name, OBU_NAME_MAX_LEN);
    SPM_OBU_obj->vehicle_type = OBU_obj->vehicle_type;
    INIT_LIST_HEAD(&SPM_OBU_obj->node);
    return SPM_OBU_obj;
}

bool SPM_OBU_obj_insert(OBU_object_t *OBU_obj, SignalRequestMessage *p_srm)
{
    bool send_flag = false;
    pthread_mutex_lock(&SPM_OBU_obj_mutex);
    SPM_OBU_obj_t *current, *safe;
    list_for_each_entry_safe(current, safe, &SPM_OBU_list_head, node)
    {
        if (strcmp(current->OBU_name, OBU_obj->OBU_name) == 0) {
            goto SPM_OBU_obj_insert_end;
        }
    }
    current = SPM_OBU_obj_new(OBU_obj);
    list_add(&current->node, &SPM_OBU_list_head);

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

            if (p_srm->requests.tab[i].request.requestType == PriorityRequestType_priorityRequest) {
                send_flag = true;
            }
        }
    }
    pthread_mutex_unlock(&SPM_OBU_obj_mutex);
    return send_flag;
}

SPM_OBU_obj_t *SPM_OBU_obj_delete(SPM_OBU_obj_t *target)
{
    SPM_OBU_obj_t *next = list_entry(target->node.next, SPM_OBU_obj_t, node);
    list_del(&target->node);
    return next;
}