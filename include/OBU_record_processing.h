#ifndef OBU_RECORD_PROCESSING_H
#define OBU_RECORD_PROCESSING_H

#include <pthread.h>
#include <signal.h>

#include "typedefine.h"

#define HASH_TABLE_SIZE 10
#define OBU_OBJECT_EXPIRE_TIME 60

extern OBU_object_t normal_OBU_list[HASH_TABLE_SIZE];
extern OBU_object_t special_OBU_list[VEHICLE_TYPE_NUMBER];

extern pthread_mutex_t mutex_normal_OBU_list[HASH_TABLE_SIZE];
extern pthread_mutex_t mutex_special_OBU_list[VEHICLE_TYPE_NUMBER];

unsigned long djb2_hash(char *);

void OBU_record_ring_pop(OBU_object_t *);

OBU_object_t *OBU_object_new(OBU_record_common_field_t *);
OBU_object_t *OBU_object_search(OBU_object_t *, char *);
OBU_object_t *normal_OBU_record_insert(OBU_record_common_field_t *);
OBU_object_t *special_OBU_record_insert(OBU_record_common_field_t *);
OBU_object_status special_OBU_list_search_status(vehicle_type_t , char *);

void OBU_object_garbage_collection_init();
void OBU_object_garbage_collection_timer(__sigval_t value);
void OBU_object_garbage_collection();
void OBU_object_print();

int V2R_msgf2OBU_record(MessageFrame *, OBU_record_common_field_t *);


#endif