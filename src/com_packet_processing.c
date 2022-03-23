#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OBU_record_processing.h"
#include "ObstacleList.h"
#include "application_registration.h"
#include "byte_processing.h"
#include "com_io.h"
#include "com_packet_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "error_status.h"
#include "log.h"
#include "msg_queue.h"
#include "server.h"
#include "threadpool.h"
#include "timer_event.h"
#include "traffic_signal_packet_rx.h"
#include "typedefine.h"

#include "error_code_user.h"
#include "j2735_codec.h"

#define CPS_ID 3
extern threadpool_t *pool;
char log_content[LOG_CONTENT_LEN + 1];

int DSRC_send_timer_handler(buffer_ring_t *buffer)
{
    buffer_t *pkg = NULL;
    char log_content[LOG_CONTENT_LEN + 1];
    pkg = buff_ring_pop(buffer);

    if (pkg != NULL && OBU_com_id != 0) {
        int ret = com_send(OBU_com_id, pkg->buff, pkg->size);
        if (ret == COM_IO_ERR) {
            log_file_write_fatal_error("OBU_j2735_tx: com_send");
        }
    }
}
void OBU_j2735_tx(uint16_t len, void *buf)
{
    char log_content[LOG_CONTENT_LEN + 1];
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(len);
    if (write_buf.content == NULL) {
        log_file_write_fatal_error("OBU_j2735_tx: malloc");
        perror("OBU_j2735_tx: malloc");
        exit(errno);
    } else {
        memset(write_buf.content, 0, len);
    }
    if (memcpy(&write_buf.content[write_buf.index], (unsigned char *) buf,
               len) == NULL) {
        log_file_write_fatal_error("OBU_j2735_tx: memcpy");
    }
    // printf("obu com id:%d\n", OBU_com_id);
    int ret = com_send(OBU_com_id, write_buf.content, len);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("OBU_j2735_tx: com_send");
    }
    if (write_buf.content != NULL) {
        free(write_buf.content);
    }
    if (buf != NULL) {
        free(buf);
    }
    return;
}
void OBU_packet_tx(uint16_t len,
                   uint8_t service_id,
                   unsigned char *specific_field)
{
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2V_COMMON_FIELD_LEN + len);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_tx: malloc");
        perror("OBU_packet_tx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2V_COMMON_FIELD_LEN + len);
    }

    // packet_len
    write_uint32_t(len + R2V_COMMON_FIELD_LEN, &write_buf);
    // device type
    write_uint8_t(DEVICE_RSU, &write_buf);
    // RSU id
    write_char(config.RSU_id, &write_buf, sizeof(config.RSU_id) - 1,
               RSU_ID_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[20];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);
    write_char(buffer, &write_buf, TIMESTAMP_LEN, TIMESTAMP_LEN);

    // position
    write_float(config.RSU_lon, &write_buf);
    write_float(config.RSU_lat, &write_buf);
    // service id
    write_uint8_t(service_id, &write_buf);

    // merge commom field & specific field
    memcpy(&write_buf.content[write_buf.index], specific_field, len);
    write_buf.index += len;

    if (config.log_OBU_packet_tx) {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "OBU packet tx: service(%d) length(%d)\n", service_id,
                 write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     write_buf.content[i]);
        }
        log_file_write(log_content);
    }

    int ret = com_send(OBU_com_id, write_buf.content, write_buf.index);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("OBU_packet_tx: com_send");
    }

    if (write_buf.content != NULL) {
        free(write_buf.content);
    }
    return;
}

// to send packet to cloud
void cloud_packet_tx(uint16_t len,
                     uint8_t service_id,
                     unsigned char *specific_field)
{
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_COMMON_FIELD_LEN + len);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("cloud_packet_tx: malloc");
        perror("cloud_packet_tx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_COMMON_FIELD_LEN + len);
    }

    // packet_len
    write_uint32_t(len + R2C_COMMON_FIELD_LEN, &write_buf);
    // device type
    write_uint8_t(DEVICE_RSU, &write_buf);
    // RSU device id
    write_char(config.RSU_id, &write_buf, sizeof(config.RSU_id) - 1,
               RSU_ID_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[20];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);
    write_char(buffer, &write_buf, TIMESTAMP_LEN, TIMESTAMP_LEN);

    // position
    write_float(config.RSU_lon, &write_buf);
    write_float(config.RSU_lat, &write_buf);
    // service id
    write_uint8_t(service_id, &write_buf);

    // merge commom field & specific field
    memcpy(&write_buf.content[write_buf.index], specific_field, len);
    write_buf.index += len;

    if (config.log_cloud_packet_tx) {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "cloud packet tx: service(%d) length(%d)\n", service_id,
                 write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     write_buf.content[i]);
        }
        log_file_write(log_content);
    }

    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "in cloud packet tx cloud_com_id is %d\n", cloud_com_id);
    log_file_write(log_content);
    int ret = com_send(cloud_com_id, write_buf.content, write_buf.index);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("cloud_packet_tx: com_send");
    }

    usleep(50000);  //直接註解com layer會錯

    if (write_buf.content != NULL) {
        free(write_buf.content);
    }
    return;
}

int cloud_packet_rx_event_handler(msg_obj_t *msg)
{
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t read_buf;
    C2R_common_field_t common_field;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(C2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("cloud_packet_rx_event_handler: malloc");
        perror("cloud_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, msg->msg, C2R_COMMON_FIELD_LEN);
    }

    read_buf.index = C2R_COMMON_FIELD_LEN;

    if (config.log_cloud_packet_rx) {
        printf("common filed.service_id:%d\r\n", common_field.service_id);
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "cloud packet rx: COMMON FIELD\n");
        for (int i = 0; i < read_buf.index; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        log_file_write(log_content);
    }

    read_buf.index = 0;

    // packet_len
    read_uint32_t(&common_field.packet_len, &read_buf);
    // device type
    read_uint8_t(&common_field.device_type, &read_buf);
    // RSU id
    read_char(common_field.RSU_id, &read_buf, RSU_ID_MAX_LEN);
    // timestamp
    read_char(common_field.timestamp, &read_buf, TIMESTAMP_LEN);
    // service id
    read_uint8_t(&common_field.service_id, &read_buf);

    /* common field value valid */
    if (common_field.packet_len < C2R_COMMON_FIELD_LEN) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_PACKET_LEN;
    }
    if (common_field.device_type < 0 ||
        common_field.device_type >= DEVICE_TYPE_NUMBER) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_DEVICE_TYPE;
    }
    if (strncmp(common_field.RSU_id, config.RSU_id, RSU_ID_MAX_LEN) != 0) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_RSU_ID;
    }
    if (common_field.service_id < 1) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_SEVICE_ID;
    }

    // pass application payload to app code
    C2R_app_section_t app_section;
    app_section.payload_len = common_field.packet_len - C2R_COMMON_FIELD_LEN;
    app_section.payload = (char *) malloc(app_section.payload_len);
    if (app_section.payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("cloud_packet_rx_event_handler: malloc");
        perror("cloud_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.payload, &msg->msg[read_buf.index],
               app_section.payload_len);
        app_section.com_id = msg->handle_id;
    }

    event_callback_t *current = &callback_list[EVENT_CLOUD_PACKET_RX];
    while (current->next != NULL) {
        if (common_field.service_id == current->next->app_id) {
            current->next->callback((void *) &app_section);  // what com_id for?
        }
        current = current->next;
    }

    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    if (app_section.payload != NULL) {
        free(app_section.payload);
    }
    return PACKET_PROCESSING_ACCEPT;
}

int OBU_packet_rx_bsm(V2R_common_field_t *common_field, msg_obj_t *msg)
{
    MessageFrame *msgf;
    int ret = j2735_msg_decode(&msgf, (uint8_t *) msg->msg, msg->msg_len, NULL);

    if (ret < 0) {
        return PACKET_NOT_J2735;
    }

    if (msgf->messageId != BasicSafetyMessage_Id) {
        J2735_FREE_MSG_FRAME(msgf);
        return PACKET_IS_J2735_BUT_NOT_BSM;
    }
    BasicSafetyMessage *bsm = msgf->u.data;

    if (!bsm->regional_option || bsm->regional.count != 1 ||
        bsm->regional.tab[0].regionId != 254 || !bsm->partII_option ||
        bsm->partII.count != 1) {
        J2735_FREE_MSG_FRAME(msgf);
        return PACKET_IS_J2735_BUT_NOT_BSM;
    }

    OctetString *reg_bsm = &bsm->regional.tab[0].u.unknown;

    common_field->packet_len = msg->msg_len;

    common_field->service_id = reg_bsm->buf[0];
    common_field->device_type = reg_bsm->buf[1];

    common_field->position_lat = bsm->coreData.lat / 10000000;
    common_field->position_lon = bsm->coreData.Long / 10000000;

    common_field->speed = bsm->coreData.speed / 50;
    common_field->direction = (u_int8_t)(bsm->coreData.heading / 3600);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int second = tv.tv_sec % 60;
    float secMark = bsm->coreData.secMark / 1000;

    if (second < secMark)
        tv.tv_sec -= 60;
    tv.tv_sec = tv.tv_sec - second + secMark;

    char buffer[20];
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
    memcpy(common_field->timestamp, buffer, TIMESTAMP_LEN);

    // strptime(common_field->timestamp, "%Y-%m-%d %H:%M:%S",
    // &record->time_stamp);

    SupplementalVehicleExtensions *sup_ext =
        bsm->partII.tab[0].u.supplementalExt;
    if (sup_ext->classification == 50) {
        common_field->vehicle_type = 2;
        strncpy(common_field->OBU_id, "bus_\0", 5);
        strncat(common_field->OBU_id, bsm->coreData.id.buf,
                bsm->coreData.id.len);
    } else if (sup_ext->classification == 60) {
        common_field->vehicle_type = 1;
        strncpy(common_field->OBU_id, "amb_\0", 5);
        strncat(common_field->OBU_id, bsm->coreData.id.buf,
                bsm->coreData.id.len);
    }

    if (common_field->device_type < 0 ||
        common_field->device_type >= DEVICE_TYPE_NUMBER) {
        J2735_FREE_MSG_FRAME(msgf);
        return PACKET_INVALID_DEVICE_TYPE;
    }
    if (common_field->vehicle_type < 0 ||
        common_field->vehicle_type >= VEHICLE_TYPE_NUMBER) {
        J2735_FREE_MSG_FRAME(msgf);
        return PACKET_INVALID_VEHICLE_TYPE;
    }
    if (common_field->service_id < 0) {  // 0 is for middleware
        J2735_FREE_MSG_FRAME(msgf);
        return PACKET_INVALID_SEVICE_ID;
    }

    common_field->payload_len = reg_bsm->len - V2R_BSM_REGIONAL_LEN;
    common_field->payload = (char *) malloc(common_field->payload_len);
    if (common_field->payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(common_field->payload, &reg_bsm->buf[V2R_BSM_REGIONAL_LEN],
               common_field->payload_len);
    }

    J2735_FREE_MSG_FRAME(msgf);
    return PACKET_IS_BSM;
}

int OBU_packet_rx_row_data(V2R_common_field_t *common_field, msg_obj_t *msg)
{
    msg_buf_t read_buf;

    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(V2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, msg->msg, V2R_COMMON_FIELD_LEN);
    }

    read_buf.index = 0;
    // packet_len
    read_uint32_t(&common_field->packet_len, &read_buf);  // 49
    // device type
    read_uint8_t(&common_field->device_type, &read_buf);
    // OBU id
    read_char(common_field->OBU_id, &read_buf, OBU_ID_MAX_LEN);
    // vehicle_type
    read_uint8_t(&common_field->vehicle_type, &read_buf);
    // timestamp
    read_char(common_field->timestamp, &read_buf, TIMESTAMP_LEN);
    // position
    read_float(&common_field->position_lon, &read_buf);
    read_float(&common_field->position_lat, &read_buf);
    // speed
    read_uint8_t(&common_field->speed, &read_buf);
    // direction
    read_uint8_t(&common_field->direction, &read_buf);
    // service id
    read_uint8_t(&common_field->service_id, &read_buf);

    free(read_buf.content);
    /* common field value valid */
    if (common_field->packet_len < V2R_COMMON_FIELD_LEN)
        return PACKET_INVALID_PACKET_LEN;

    if (common_field->device_type < 0 ||
        common_field->device_type >= DEVICE_TYPE_NUMBER) {
        return PACKET_INVALID_DEVICE_TYPE;
    }
    if (common_field->vehicle_type < 0 ||
        common_field->vehicle_type >= VEHICLE_TYPE_NUMBER) {
        return PACKET_INVALID_VEHICLE_TYPE;
    }
    if (common_field->service_id < 0) {  // 0 is for middleware
        return PACKET_INVALID_SEVICE_ID;
    }

    common_field->payload_len = common_field->packet_len - V2R_COMMON_FIELD_LEN;
    common_field->payload = (char *) malloc(common_field->payload_len);
    if (common_field->payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(common_field->payload, &read_buf.content[read_buf.index],
               common_field->payload_len);
    }

    return PACKET_NOT_J2735;
}

int OBU_packet_rx_event_handler(msg_obj_t *msg)
{
    printf("get in obu rx handler\n\r");

    // event_callback_t *current_c = &callback_list[EVENT_CAMERA_PACKET_RX];
    // //pthread_t APP_thread;
    // while (current_c->next != NULL) {
    //     if (5 == current_c->next->app_id) {
    //         if(threadpool_add(pool, current_c->next->callback, NULL, 0) !=
    //         0){
    //             printf("threadpool adding error!\n");//ERROR
    //         }
    //         // current_c->next->callback(NULL);
    //     }
    //     current_c = current_c->next;
    // }
    printf("handler complete\n");

    char log_content[LOG_CONTENT_LEN + 1];
    if (config.log_OBU_packet_rx) {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "OBU packet rx: COMMON FIELD\n");
        for (int i = 0; i < msg->msg_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ", msg->msg[i]);
        }
        log_file_write(log_content);
    }

    V2R_common_field_t common_field;

    int ret = OBU_packet_rx_bsm(&common_field, msg);
    if (ret < 0)
        return ret;

    if (ret != PACKET_IS_BSM)
        ret = OBU_packet_rx_row_data(&common_field, msg);
    if (ret < 0)
        return ret;

    if (common_field.device_type < 0 ||
        common_field.device_type >= DEVICE_TYPE_NUMBER) {
        return PACKET_INVALID_DEVICE_TYPE;
    }
    if (common_field.vehicle_type < 0 ||
        common_field.vehicle_type >= VEHICLE_TYPE_NUMBER) {
        return PACKET_INVALID_VEHICLE_TYPE;
    }
    if (common_field.service_id < 0) {  // 0 is for middleware
        return PACKET_INVALID_SEVICE_ID;
    }

    // 這裡是用來偵測dsrc是否還活著
    //   if(common_field.service_id==0){ //dsrc heart beat packet
    /*       printf("dsrc alive and postpone the timer handle execution\r\n");
           log_file_write("dsrc alive and postpone the timer handle
       execution\r\n"); set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
           clear_dsrc_error();

           return PACKET_PROCESSING_ACCEPT;
       }*/


    OBU_record_t *record = (OBU_record_t *) malloc(sizeof(OBU_record_t));
    OBU_object_t *object = NULL;
    if (record == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(record, 0, sizeof(OBU_record_t));
    }


    // convert common field to OBU record
    V2R_packet2OBU_record(&common_field, record);


    // record和obu object都有obu id這樣才知道要把packet裡面節錄出來的record資料
    //放到obu list裡面的哪個obu object
    switch (record->vehicle_type) {
    /* Ambulance */
    case VEHICLE_AMBULANCE:
        object = special_OBU_record_insert(record);
        break;

    /* bus */
    case VEHICLE_BUS:
        object = special_OBU_record_insert(record);
        break;

    /* normal vehicle */
    case VEHICLE_NORMAL:
        object = normal_OBU_record_insert(record);
        break;
    }
    if (record != NULL) {
        free(record);
    }

    OBU_object_print();

    V2R_app_section_t app_section;

    app_section.payload_len = common_field.payload_len;

    app_section.payload = (char *) malloc(app_section.payload_len);
    if (app_section.payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.payload, &common_field.payload,
               app_section.payload_len);
        app_section.com_id = msg->handle_id;
    }

    app_section.OBU_object = (OBU_object_t *) malloc(sizeof(OBU_object_t));
    if (app_section.OBU_object == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.OBU_object, object, sizeof(OBU_object_t));
    }

    event_callback_t *current = &callback_list[EVENT_OBU_PACKET_RX];
    while (current->next != NULL) {
        if (common_field.service_id == current->next->app_id) {
            current->next->callback((void *) &app_section);
        }
        current = current->next;
    }


    // free resource just
    if (common_field.payload != NULL) {
        free(common_field.payload);
    }
    if (app_section.payload != NULL) {
        free(app_section.payload);
    }
    if (app_section.OBU_object != NULL) {
        free(app_section.OBU_object);
    }
    return PACKET_PROCESSING_ACCEPT;
}

double Smart_AVI_packet_rx_event_handler(msg_obj_t *msg)
{
    int cnt = 0;
    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(25600);
    if (read_buf.content == NULL) {
        // log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("Smart_AVI_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        memcpy(read_buf.content, msg->msg, msg->msg_len);
    }
    ObstacleList *obstaclelist = (ObstacleList *) malloc(sizeof(ObstacleList));
    int hour, min;
    float second;

    read_uint32_t(&obstaclelist->dirct, &read_buf);
    read_uint32_t(&hour, &read_buf);
    read_uint32_t(&min, &read_buf);
    read_float(&second, &read_buf);
    read_uint32_t(&obstaclelist->count, &read_buf);

    obstaclelist->tab =
        (Obstacle *) calloc(sizeof(Obstacle), obstaclelist->count);

    if (obstaclelist->tab == NULL) {
        perror("Smart_AVI_packet_rx_event_handler: malloc");
        exit(errno);
    }
    for (int i = 0; i < obstaclelist->count; i++) {
        read_double(&obstaclelist->tab[i].lat, &read_buf);
        read_double(&obstaclelist->tab[i].Long, &read_buf);
        read_double(&obstaclelist->tab[i].elev, &read_buf);

        read_uint32_t(&obstaclelist->tab[i].laneID, &read_buf);
        read_uint32_t(&obstaclelist->tab[i].ObstacleID, &read_buf);
        read_uint32_t(&obstaclelist->tab[i].description, &read_buf);

        obstaclelist->tab[i].length = 0;
        obstaclelist->tab[i].width = 0;
        obstaclelist->tab[i].hour = hour;
        obstaclelist->tab[i].minute = min;
        obstaclelist->tab[i].second = second;

        read_buf.index += 24;
    }
    event_callback_t *current = &callback_list[EVENT_CAMERA_PACKET_RX];
    while (current->next != NULL) {
        if (CPS_ID == current->next->app_id) {
            // if(threadpool_add(pool, current->next->callback, obstaclelist, 0)
            // != 0){
            //     printf("threadpool adding error!\n");//ERROR
            // }
            current->next->callback((void *) obstaclelist);
        }
        current = current->next;
    }
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}
int Is_Heartbeat(msg_obj_t *msg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    msg_buf_t read_buf;
    V2R_common_field_t common_field;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(V2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, msg->msg, V2R_COMMON_FIELD_LEN);
    }
    read_buf.index = 0;
    read_uint32_t(&common_field.packet_len, &read_buf);
    // service id
    read_uint8_t(&common_field.service_id, &read_buf);
    if (common_field.service_id == 0 &&
        common_field.packet_len == 512) {  // dsrc heart beat packet
        printf("dsrc alive and postpone the timer handle execution\r\n");
        log_file_write(
            "dsrc alive and postpone the timer handle execution\r\n");
        // set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
        // clear_dsrc_error();
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return 1;
    }
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}