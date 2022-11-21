#ifndef SPM_OBU_LIST_H
#define SPM_OBU_LIST_H

#include "typedefine.h"
#include <pthread.h>

typedef struct SPM_VehicleID {
    VehicleID_choice choice;
    union {
        uint8_t buf[4]; /* TemporaryID (4..4) */
        uint32_t stationID; /* StationID (0..4294967295) */
    } u;
} SPM_VehicleID;

typedef struct SPM_OBU_obj {
    SPM_VehicleID id;
    BasicVehicleRole role;
    char OBU_name[OBU_NAME_MAX_LEN + 1];  //+1 if for \0
    uint8_t vehicle_type;
    time_t time_second;
    SignalRequestPackage sigRequestList[SignalRequestList_MAX_SIZE];
    int sigRequest_count;
    struct SPM_OBU_obj *next;
}SPM_OBU_obj_t;

extern SPM_OBU_obj_t *SPM_OBU_obj_head;
extern pthread_mutex_t SPM_OBU_obj_mutex;

void SPM_OBU_obj_insert(OBU_object_t *OBU_obj, SignalRequestMessage* srm);
void SPM_OBU_obj_delete(char *OBU_name);
#endif