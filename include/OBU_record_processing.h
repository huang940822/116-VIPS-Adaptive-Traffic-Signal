#ifndef OBU_RECORD_PROCESSING_H
#define OBU_RECORD_PROCESSING_H

#include <pthread.h>

#include "typedefine.h"

#define HASH_TABLE_SIZE 10
#define OBU_OBJECT_EXPIRE_TIME 60

extern OBU_object_t normal_OBU_list[HASH_TABLE_SIZE];
extern OBU_object_t special_OBU_list[VEHICLE_TYPE_NUMBER];

extern pthread_mutex_t mutex_normal_OBU_list[HASH_TABLE_SIZE];
extern pthread_mutex_t mutex_special_OBU_list[VEHICLE_TYPE_NUMBER];

unsigned long djb2_hash(char *);

void OBU_record_ring_pop(OBU_object_t *);

OBU_object_t *OBU_object_new(char *, uint8_t);
OBU_object_t *OBU_object_search(OBU_object_t *, char *);
OBU_object_t *normal_OBU_record_insert(OBU_record_t *);
OBU_object_t *special_OBU_record_insert(OBU_record_t *);

void OBU_object_garbage_collection();
void OBU_object_print();

void V2R_packet2OBU_record(V2R_common_field_t *packet, OBU_record_t *record);

#endif