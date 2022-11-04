#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OBU_record_processing.h"
#include "application_registration.h"
#include "config.h"
#include "error_status.h"
#include "j2735_bsm.h"
#include "j2735_msg.h"
#include "j2735_srm.h"
#include "log.h"
#include "timer_event.h"
#include "typedefine.h"
#include "sys/time.h"

timer_t OBU_list_garbage_collection_timer_id;

OBU_object_t normal_OBU_list[HASH_TABLE_SIZE];
OBU_object_t special_OBU_list[VEHICLE_TYPE_NUMBER];

pthread_mutex_t mutex_normal_OBU_list[HASH_TABLE_SIZE] =
    PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_special_OBU_list[VEHICLE_TYPE_NUMBER] =
    PTHREAD_MUTEX_INITIALIZER;

/*****************************************************************************
** Function:    djb2_hash
** Description: Hash function.
** Parameter:   str: string to hash
** Return:      hash code
******************************************************************************/
unsigned long djb2_hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash % HASH_TABLE_SIZE;
}

/*****************************************************************************
** Function:    OBU_record_ring_empty
** Description: Check if OBU record ring is empty.
** Parameter:   front: front of ring
**              back : back of ring
** Return:      true : ring is empty
**              false: ring is not empty
******************************************************************************/
bool OBU_record_ring_empty(uint8_t front, uint8_t back)
{
    return (front == back);
}

/*****************************************************************************
** Function:    OBU_record_ring_full
** Description: Check if OBU record ring is full.
** Parameter:   front: front of ring
**              back : back of ring
** Return:      true : ring is full
**              false: ring is not full
******************************************************************************/
bool OBU_record_ring_full(uint8_t front, uint8_t back)
{
    return ((back + 1) % OBU_RECORD_RING_CAPACITY == front);
}

/*****************************************************************************
** Function:    OBU_record_ring_push
** Description: Push a record in OBU record ring.
** Parameter:   record: record to push
**              object: OBU obj with the same OBU id as the record
** Return:      none
******************************************************************************/
void OBU_record_ring_push(OBU_record_t *record, OBU_object_t *object)
{
    object->record_ring.last_record_pointer =
        (object->record_ring.last_record_pointer + 1) %
        OBU_RECORD_RING_CAPACITY;
    object->record_ring.length = object->record_ring.length + 1;
    object->record_ring.last_record_pointer =
        (object->record_ring.last_record_pointer + 1) %
        OBU_RECORD_RING_CAPACITY;
    memcpy(&object->record_ring.record[object->record_ring.last_record_pointer],
           record, sizeof(OBU_record_t));
}

/*****************************************************************************
** Function:    OBU_record_ring_pop
** Description: Discard a record in OBU record ring.
** Parameter:   object: OBU obj to discard a record
** Return:      none
******************************************************************************/
void OBU_record_ring_pop(OBU_object_t *object)
{
    object->record_ring.first_record_pointer =
        (object->record_ring.first_record_pointer + 1) %
        OBU_RECORD_RING_CAPACITY;
    object->record_ring.length = object->record_ring.length - 1;
}

/*****************************************************************************
** Function:    OBU_object_new
** Description: Create a new OBU obj.
** Parameter:   str: OBU id of new OBU obj
** Return:      object: address of new OBU obj
******************************************************************************/
OBU_object_t *OBU_object_new(char *str, uint8_t type)
{
    OBU_object_t *object = (OBU_object_t *) malloc(sizeof(OBU_object_t));
    if (object == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_object_new: malloc");
        perror("OBU_object_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(object, 0, sizeof(OBU_object_t));
    }
    strncpy(object->OBU_name, str, OBU_NAME_MAX_LEN);
    object->vehicle_type = type;
    object->record_ring.first_record_pointer = 0;
    object->record_ring.last_record_pointer = 0;
    object->record_ring.length = 0;
    object->private_space = (app_private_space_t *) malloc(
        sizeof(app_private_space_t));  //看不懂這一段 為何要乘以app_num+1
    if (object->private_space == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_object_new: malloc");
        perror("OBU_object_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(object->private_space, 0,
               sizeof(app_private_space_t));
    }
    object->prev = NULL;
    object->next = NULL;
    return object;
}

/*****************************************************************************
** Function:    OBU_object_search
** Description: Search a OBU obj with specific OBU id.
** Parameter:   OBU_list_head: head node of OBU list where the OBU obj placed
**              str: OBU id of OBU obj to find
** Return:      object: address of OBU obj
**              NULL: OBU obj not found
******************************************************************************/
OBU_object_t *OBU_object_search(OBU_object_t *OBU_list_head, char *str)
{
    OBU_object_t *current = OBU_list_head->next;
    while (current != OBU_list_head) {
        if (strncmp(current->OBU_name, str, OBU_NAME_MAX_LEN) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/*****************************************************************************
** Function:    normal_OBU_record_insert
** Description: Insert a normal OBU record in normal OBU list.
** Parameter:   record: OBU record to insert
** Return:      object: address of OBU obj where record insert
******************************************************************************/
OBU_object_t *normal_OBU_record_insert(OBU_record_t *record)  //這裡用hash table
{
    int hash_code = djb2_hash(
        record->OBU_name);  // hash code is array index for having use mod
    pthread_mutex_lock(&mutex_normal_OBU_list[hash_code]);
    OBU_object_t *object =
        OBU_object_search(&normal_OBU_list[hash_code], record->OBU_name);

    if (object == NULL) { /* new OBU object */
        object = OBU_object_new(record->OBU_name, VEHICLE_NORMAL);

        /* insert OBU record */  //如果世新的object 那record ring一定是空的
                                 //似乎沒有檢查的必要 直接push進去就好？
        if (!OBU_record_ring_full(object->record_ring.first_record_pointer,
                                  object->record_ring.last_record_pointer)) {
            OBU_record_ring_push(record, object);
        } else {
            OBU_record_ring_pop(object);  /// pop純粹就是丟掉嗎？
            OBU_record_ring_push(record, object);
        }

        /* insert at head */
        object->next = normal_OBU_list[hash_code].next;
        object->prev = &normal_OBU_list[hash_code];

        normal_OBU_list[hash_code].next->prev = object;
        normal_OBU_list[hash_code].next = object;

    } else { /* OBU object exist */
        /* insert OBU record */
        if (!OBU_record_ring_full(object->record_ring.first_record_pointer,
                                  object->record_ring.last_record_pointer)) {
            OBU_record_ring_push(record, object);
        } else {
            //滿了 要先pop再push
            OBU_record_ring_pop(object);
            OBU_record_ring_push(record, object);
        }
    }
    pthread_mutex_unlock(&mutex_normal_OBU_list[hash_code]);
    return object;
}

/*****************************************************************************
** Function:    special_OBU_record_insert
** Description: Insert a special OBU record in special OBU list.
** Parameter:   record: OBU record to insert
** Return:      object: address of OBU obj where record insert
******************************************************************************/
OBU_object_t *special_OBU_record_insert(OBU_record_t *record)
{
    uint8_t type = record->vehicle_type;
    pthread_mutex_lock(&mutex_special_OBU_list[type]);
    OBU_object_t *object =
        OBU_object_search(&special_OBU_list[type], record->OBU_name);

    if (object == NULL) { /* new OBU object */ 
        object = OBU_object_new(record->OBU_name, type);
        /* insert OBU record */
        OBU_record_ring_push(record, object);
        /* insert at head */
        object->next = special_OBU_list[type].next;
        object->prev = &special_OBU_list[type];

        special_OBU_list[type].next->prev = object;
        special_OBU_list[type].next = object;
    } else { /* OBU object exist */
        /* insert OBU record */
        if (OBU_record_ring_full(object->record_ring.first_record_pointer,
                                  object->record_ring.last_record_pointer)) {
            OBU_record_ring_pop(object);
        }
        OBU_record_ring_push(record, object);
        /* move target to head */
        if (special_OBU_list[type].next != object) {
            /* remove target */
            object->next->prev = object->prev;
            object->prev->next = object->next;

            /* insert to head */
            object->next = special_OBU_list[type].next;
            object->prev = &special_OBU_list[type];

            special_OBU_list[type].next->prev = object;
            special_OBU_list[type].next = object;
        }
    }
    pthread_mutex_unlock(&mutex_special_OBU_list[type]);
    return object;
}

static int yday2month_day(struct tm *timeinfo, int yday)
{
    int months_arr[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, month = 0;
    int year = timeinfo->tm_year + 1900;

    if ((year % 400 == 0 || year % 100 != 0) && (year % 4 == 0))
        months_arr[1]++;

    for (month; month < 12; month++) {
        if (yday < months_arr[month]) {
            break;
        }
        yday -= months_arr[month];
    }
    
    if (month >= 12 || timeinfo->tm_mon != month)
        return -1;
    timeinfo->tm_mday = yday;
    return 1;
}

int V2R_msgf2OBU_record(MessageFrame *msgf, OBU_record_t *record)
{
    switch (msgf->messageId) {
    case BasicSafetyMessage_Id: {
        BasicSafetyMessage *bsm = msgf->u.data;
        if (bsm->partII_option != TRUE || bsm->partII.count != 1 || bsm->partII.tab[0].partII_Id != SupplementalVehicleExt) {
            return -1;
        }
        SupplementalVehicleExtensions *sup_ext = bsm->partII.tab[0].u.supplementalExt;
        if (sup_ext->classification_option != TRUE) {
            return -1;
        }
        switch (sup_ext->classification) {
        case 50:
            strcpy(record->OBU_name, "bus_");
            strncat(record->OBU_name, bsm->coreData.id.buf, 4);
            record->vehicle_type = VEHICLE_BUS;
            break;
        case 60:
            strcpy(record->OBU_name, "amb_");
            strncat(record->OBU_name, bsm->coreData.id.buf, 4);
            record->vehicle_type = VEHICLE_AMBULANCE;
            break;
        default:
            break;
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        int second = tv.tv_sec % 60;
        int secMark = bsm->coreData.secMark / 1000;
        if (second > secMark && (second - secMark) > 30)
            tv.tv_sec -= 60;
        tv.tv_sec = tv.tv_sec - second + secMark;
        record->time_second = mktime(localtime(&tv.tv_sec));

        record->position_lat = bsm->coreData.lat / 10000000.0;
        record->position_lon = bsm->coreData.Long / 10000000.0;
        
        record->speed = bsm->coreData.speed * 0.072;
        record->direction = bsm->coreData.heading / 3600;
        record->direction &= 0b111;
    } break;
    case SignalRequestMessage_Id: {
        SignalRequestMessage *srm = msgf->u.data;
        if (srm->requestor.id.choice != VehicleID_entityID || srm->requestor.type_option != TRUE ||
            srm->requestor.type.hpmsType_option != TRUE || srm->requestor.type.hpmsType != VehicleType_car ||
            srm->requestor.position_option != TRUE || srm->requestor.position.speed_option != TRUE) {
            return -1;
        }
        RequestorDescription *requestor = &srm->requestor;
        switch (srm->requestor.type.role) {
        case BasicVehicleRole_ambulance:
            strcpy(record->OBU_name, "amb_");
            strncat(record->OBU_name, requestor->id.u.entityID.buf, 4);
            record->vehicle_type = VEHICLE_AMBULANCE;
            break;
        default:
            return -1;
            break;
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm *timeinfo;
        timeinfo = localtime(&tv.tv_sec);
        
        int yday = srm->timeStamp / 1440, dmin = srm->timeStamp % 1440;

        if (yday2month_day(timeinfo, yday) == -1) {printf("rfrfrf\n");
            return -1;
        }
            
        timeinfo->tm_hour = dmin / 60;
        timeinfo->tm_min = dmin % 60;
        timeinfo->tm_sec = srm->second / 1000;
        timeinfo->tm_isdst = -1;
        record->time_second = mktime(timeinfo);

        record->position_lat = requestor->position.position.lat / 10000000.0;
        record->position_lon = requestor->position.position.Long / 10000000.0;
        record->speed = requestor->position.speed.speed * 0.072;
        record->direction = requestor->position.heading / 3600;
        record->direction &= 0b111;
    } break;
    default:
        return -1;
        break;
    }
    return 1;
}

//把obu packet資料讀到obu object
void V2R_packet2OBU_record(V2R_common_field_t *packet, OBU_record_t *record)
{
    strncpy(record->OBU_name, packet->OBU_name, OBU_NAME_MAX_LEN);
    record->time_second = mktime(&packet->timestamp);
    record->position_lon = packet->position_lon;
    record->position_lat = packet->position_lat;
    record->speed = packet->speed;
    record->direction = packet->direction;
    record->vehicle_type = packet->vehicle_type;
}

void OBU_object_garbage_collection_init()
{
    for (int i = 0;i < HASH_TABLE_SIZE;i++) {
        normal_OBU_list[i].next = &normal_OBU_list[i];
        normal_OBU_list[i].prev = &normal_OBU_list[i];
    }
    for (int i = 0;i < VEHICLE_TYPE_NUMBER;i++) {
        special_OBU_list[i].next = &special_OBU_list[i];
        special_OBU_list[i].prev = &special_OBU_list[i];
    }
    create_timer(&OBU_list_garbage_collection_timer_id, NULL, OBU_object_garbage_collection_timer);
    set_timer(OBU_list_garbage_collection_timer_id, 10, 0, 10, 0);
}

void OBU_object_garbage_collection_timer(__sigval_t value)
{
    if (config.log_middleware_timer_event) {
        log_file_write("timer event: OBU list garbage collection");
    }

    OBU_object_garbage_collection();
    OBU_object_print();
}

void OBU_object_garbage_collection()
{
    time_t current_time;
    time(&current_time);
    OBU_object_t *current = NULL;
    OBU_object_t *target = NULL;
    // normal OBU list
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        pthread_mutex_lock(&mutex_normal_OBU_list[i]);
        current = normal_OBU_list[i].next;

        while (current != &normal_OBU_list[i]) {
            target = NULL;
            if ((current_time -
                 current->record_ring
                     .record[current->record_ring.last_record_pointer]
                     .time_second) > OBU_OBJECT_EXPIRE_TIME) {
                target = current;
                target->next->prev = target->prev;
                target->prev->next = target->next;
            }
            current = current->next;
            if (target) {
                free(target->private_space);
                free(target);
            }
        }
        pthread_mutex_unlock(&mutex_normal_OBU_list[i]);
    }
    // special OBU list
    for (int i = 0; i < VEHICLE_TYPE_NUMBER; i++) {
        pthread_mutex_lock(&mutex_special_OBU_list[i]);
        current = special_OBU_list[i].next;
        while (current != &special_OBU_list[i]) {
            target = NULL;
            if ((current_time -
                 current->record_ring
                     .record[current->record_ring.last_record_pointer]
                     .time_second) > OBU_OBJECT_EXPIRE_TIME) {
                target = current;
                target->next->prev = target->prev;
                target->prev->next = target->next;
            }
            current = current->next;
            if (target) {
                free(target->private_space);
                free(target);
            }
        }
        pthread_mutex_unlock(&mutex_special_OBU_list[i]);
    }
}

void OBU_object_print()
{
    if (config.log_OBU_list == 0) {
        return;
    }

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "OBU list: ");

    // normal OBU list
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\nnormal_OBU_list[%d]: ", i);

        pthread_mutex_lock(&mutex_normal_OBU_list[i]);
        OBU_object_t *current = normal_OBU_list[i].next;
        while (current != &normal_OBU_list[i]) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%s(%ld)-> ",
                     current->OBU_name,
                     current->record_ring
                         .record[current->record_ring.last_record_pointer]
                         .time_second);
            current = current->next;
        }
        pthread_mutex_unlock(&mutex_normal_OBU_list[i]);
    }
    // special OBU list
    for (int i = 0; i < VEHICLE_TYPE_NUMBER; i++) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\nspecial_OBU_list[%d]: ", i);

        pthread_mutex_lock(&mutex_special_OBU_list[i]);
        OBU_object_t *current = special_OBU_list[i].next;
        while (current != &special_OBU_list[i]) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%s(%ld)-> ",
                     current->OBU_name,
                     current->record_ring
                         .record[current->record_ring.last_record_pointer]
                         .time_second);
            current = current->next;
        }
        pthread_mutex_unlock(&mutex_special_OBU_list[i]);
    }
    log_file_write(log_content);
    return;
}