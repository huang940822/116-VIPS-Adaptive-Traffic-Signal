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
#include "external_app_proxy_server.h"
#include "external_app_proxy_callback_msg_forward.h"
#include "error_code_user.h"
#include "error_code_enum.h"
#include "j2735_codec.h"

LOG_USE_MODULE(MIDDLEWARE_COM);

int cb_counter;

#define CPS_ID 3

extern threadpool_t *pool;

int DSRC_send_timer_handler(buffer_ring_t *buffer)
{
    buffer_t *pkg = NULL;
    char log_content[LOG_CONTENT_LEN + 1];
    pkg = buff_ring_pop(buffer);

    if (pkg != NULL && OBU_com_id != 0) {
        int ret = com_send(OBU_com_id, pkg->buff, pkg->size);
        if (ret == COM_IO_ERR) {
            LOG_MSG_FATAL("OBU_j2735_tx: com_send");
        }
    }
}
//向OBU發送符合j2735規範訊息
void OBU_j2735_tx(DSRCmsgID magId, void *data)
{
    int buf_len;
    uint8_t *buf;
    J2735CodecErr err;
    char errmsg_buf[ERR_MSG_SZ];

    MessageFrame msgf;
    memset(&msgf, 0, sizeof(msgf));
    memset(&err, 0, sizeof(J2735CodecErr));

    err.msg_size = ERR_MSG_SZ;
    err.msg = errmsg_buf;

    msgf.messageId = magId;
    msgf.u.data = data;
    buf_len = j2735_msg_encode(&buf, &msgf, &err);

    if (buf_len <= 0) {
        LOG_MSG_INFO("failed to encode msg");
        LOG_MSG_INFO("  [error msg] %s", err.msg);
    } else {

        int ret = com_send(OBU_com_id, buf, buf_len);
        if (ret == COM_IO_ERR) {
            LOG_MSG_FATAL("OBU_j2735_tx: com_send");
        }
    }

    j2735_buf_free(buf);
    return;
}
//以雲端封包傳送訊息至OBU
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
        LOG_MSG_FATAL("OBU_packet_tx: malloc");
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
    write_char(config.RSU_name, &write_buf, sizeof(config.RSU_name) - 1,
               RSU_NAME_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm localTime;
    char buffer[20];
    time(&rawtime);
    localtime_r(&rawtime, &localTime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &localTime);
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
        LOG_MSG_APPEND(log_content, "OBU packet tx: service(%d) length(%d)\n", service_id, write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            LOG_MSG_APPEND(log_content, "%x ", write_buf.content[i]);
        }
        LOG_MSG_INFO(log_content);
    }
    //送出OBU packet
    int ret = com_send(OBU_com_id, write_buf.content, write_buf.index);
    if (ret == COM_IO_ERR) {
        //傳送過程出錯
        LOG_MSG_FATAL("OBU_packet_tx: com_send");
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
        LOG_MSG_FATAL("cloud_packet_tx: malloc");
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
    write_char(config.RSU_name, &write_buf, sizeof(config.RSU_name) - 1,
               RSU_NAME_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm localTime;
    char buffer[20];
    time(&rawtime);
    localtime_r(&rawtime, &localTime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &localTime);
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
        LOG_MSG_APPEND(log_content, "cloud packet tx: service(%d) length(%d)\n", service_id, write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            LOG_MSG_APPEND(log_content, "%x ", write_buf.content[i]);
        }
        LOG_MSG_INFO(log_content);
    }
    LOG_MSG_INFO("in cloud packet tx cloud_com_id is %d", cloud_com_id);

    //send packet to TCP or UDP (todo: add http REST API version for transmit (Osborn 20240829) )
    int ret = com_send(cloud_com_id, write_buf.content, write_buf.index);
    if (ret == COM_IO_ERR) {
        LOG_MSG_FATAL("cloud_packet_tx: com_send");
    }

    //usleep(50000);  //直接註解com layer會錯  //學陽測試時發現可以註解掉

    if (write_buf.content != NULL) {
        free(write_buf.content);
    }
    return;
}
//RSU從雲端收到封包
int cloud_packet_rx_event_handler(msg_obj_t *msg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    msg_buf_t read_buf;
    C2R_common_field_t common_field;

    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(C2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        LOG_MSG_FATAL("cloud_packet_rx_event_handler: malloc");
        perror("cloud_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, msg->msg, C2R_COMMON_FIELD_LEN);
    }
    read_buf.index = C2R_COMMON_FIELD_LEN;

    if (config.log_cloud_packet_rx) {
        LOG_MSG_TRACE("common filed.service_id:%d", common_field.service_id);
        memset(log_content, 0, sizeof(log_content));
        LOG_MSG_APPEND(log_content, "cloud packet rx: COMMON FIELD\n");
        for (int i = 0; i < read_buf.index; i++) {
            LOG_MSG_APPEND(log_content, "%x ", read_buf.content[i]);
        }
        LOG_MSG_INFO(log_content);
    }

    read_buf.index = 0;

    // packet_len
    read_uint32_t(&common_field.packet_len, &read_buf);
    // device type
    read_uint8_t(&common_field.device_type, &read_buf);
    // RSU id
    read_char(common_field.RSU_name, &read_buf, RSU_NAME_MAX_LEN);
    // timestamp
    read_char(common_field.timestamp, &read_buf, TIMESTAMP_LEN);
    // service id
    read_uint8_t(&common_field.service_id, &read_buf);

    //檢查雲端封包是否完整
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
    if (strncmp(common_field.RSU_name, config.RSU_name, RSU_NAME_MAX_LEN) != 0) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_RSU_NAME;
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
        LOG_MSG_FATAL("cloud_packet_rx_event_handler: malloc");
        perror("cloud_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.payload, &msg->msg[read_buf.index],
               app_section.payload_len);
        //app_section.com_id = msg->handle_id;
    }

    /* since now dispatcher, ea_app_proxy, command_buf_send(),
    * all might read/write callback_list, we add a mutex_lock */
    // pthread_mutex_lock(&mutex_callback_list);

    event_callback_t *current = &callback_list[EVENT_CLOUD_PACKET_RX];
    while (current->next != NULL) {
        if (current->next->event_callback_id.choice == event_callback_id_app_id &&
            common_field.service_id == current->next->event_callback_id.u.app_id)
        {
            proxy_handling_app_p = current->next->app_obj_p;
            current->next->callback((void *) &app_section);  // what com_id for?
        }
        current = current->next;
    }

    // pthread_mutex_unlock(&mutex_callback_list);

    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    if (app_section.payload != NULL) {
        free(app_section.payload);
    }
    return PACKET_PROCESSING_ACCEPT;
}

static inline __attribute__((always_inline))
void get_payload(V2R_app_section_t *app_section, MessageFrame *msgf)
{
    switch (msgf->messageId)
    {
    case BasicSafetyMessage_Id: {
        BasicSafetyMessage *bsm = msgf->u.data;
        if (bsm->regional_option != TRUE || bsm->regional.count != 1 ||
                bsm->regional.tab[0].regionId != 254)
            return;
        app_section->payload = bsm->regional.tab[0].u.unknown.buf;
        app_section->payload_len = bsm->regional.tab[0].u.unknown.len;
    } break;
    default:
        break;
    }
}

static inline __attribute__((always_inline))
void fill_V2R_self_defined_section(V2R_self_defined_section_t *self_section_p,
                                   OBU_object_t *OBU_object_p,
                                   V2R_app_section_t *app_section_p)
{
    strncpy(self_section_p->obu_name,
            OBU_object_p->OBU_name, OBU_NAME_MAX_LEN);
    self_section_p->vehical_type = OBU_object_p->vehicle_type;

    uint8_t last_record_pointer = OBU_object_p->record_ring.last_record_pointer;
    OBU_record_t *last_record_p = &(OBU_object_p->record_ring.record[last_record_pointer]);
    self_section_p->time_second = last_record_p->time_second;
    self_section_p->lon = last_record_p->position_lon;
    self_section_p->lat = last_record_p->position_lat;
    self_section_p->speed = last_record_p->speed;
    self_section_p->direction = last_record_p->direction;

    self_section_p->msgID = app_section_p->msgID;
    if( self_section_p->msgID == BasicSafetyMessage_Id){
        self_section_p->data_len = app_section_p->payload_len;
        //self_section_p->data = app_section_p->payload;
    }
    else{
        self_section_p->data_len = 0;
        //self_section_p->data = app_section_p->data;
    }

    if(self_section_p->vehical_type == VEHICLE_AMBULANCE){
        ;//currently evsp_on_duty_flag... is directly read from payload_len
    }
    else if( self_section_p->vehical_type == VEHICLE_BUS){
        ;//currently tsp_passenger_num... is not in use
    }
    else{
        ;//currently no other vehical_type
    }
}

//OBU傳雲端封包給RSU
int OBU_packet_rx_event_handler(msg_obj_t *msg)
{
    // event_callback_t *current_c = &callback_list[EVENT_CAMERA_PACKET_RX];
    // //pthread_t APP_thread;
    // while (current_c->next != NULL) {
    //     if (5 == current_c->next->app_id) {
    //         if(threadpool_add(pool, current_c->next->callback, NULL, 0) !=
    //         0){
    //             LOG_MSG_TRACE("threadpool adding error!");//ERROR
    //         }
    //         // current_c->next->callback(NULL);
    //     }
    //     current_c = current_c->next;
    // }
    // LOG_MSG_TRACE("handler complete");

    char log_content[LOG_CONTENT_LEN + 1];
    if (config.log_OBU_packet_rx) {
        memset(log_content, 0, sizeof(log_content));
        LOG_MSG_APPEND(log_content, "OBU packet rx: COMMON FIELD\n");
        for (int i = 0; i < msg->msg_len; i++) {
            LOG_MSG_APPEND(log_content, "%x ", msg->msg[i]);
        }
        LOG_MSG_INFO(log_content);
    }

    MessageFrame *msgf = NULL;
    int ret = j2735_msg_decode(&msgf, (uint8_t *) msg->msg, msg->msg_len, NULL);
    if (ret < 0) {
        return PACKET_NOT_J2735;
    }

    OBU_record_common_field_t record;
    OBU_object_t *object = NULL;
    memset(&record, 0, sizeof(OBU_record_common_field_t));

    // convert msgf to OBU record
    if (V2R_msgf2OBU_record(msgf, &record) == -1) {
        LOG_MSG_TRACE("V2R_msgf2OBU_record fail");
        J2735_FREE_MSG_FRAME(msgf);
        return -1;
    }

    // record和obu object都有obu id這樣才知道要把packet裡面節錄出來的record資料
    //放到obu list裡面的哪個obu object
    switch (record.vehicle_type) {
    /* Ambulance */ /* bus */
    case VEHICLE_AMBULANCE: case VEHICLE_BUS: case VEHICLE_FIRE_TRUCK: case VEHICLE_POLICE_CAR:
        object = special_OBU_record_insert(&record);
        break;
    /* normal vehicle */
    case VEHICLE_NORMAL:
        object = normal_OBU_record_insert(&record);
        break;
    default:
        break;
    }

    OBU_object_print();

    V2R_app_section_t app_section;
    memset(&app_section, 0, sizeof(V2R_app_section_t));
    //記錄從OBU收到的封包 (OBU to RSU)
    app_section.msgID = msgf->messageId;
    app_section.data = msgf->u.data;
    app_section.OBU_object = (OBU_object_t *) malloc(sizeof(OBU_object_t));
    if (app_section.OBU_object == NULL) {
        set_memory_error();
        LOG_MSG_FATAL("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.OBU_object, object, sizeof(OBU_object_t));
    }

    get_payload(&app_section, msgf);

    void* callback_parameter_pointer = 0;
    #if FORWARD_SAME_FORMAT_OBU_MSG_TO_EA
        /* 有保留能傳送 struct: V2R_app_section_t 給外部 APP 的 code */
        wrapper_arg_for_obu_packet_t wrapper_arg_for_obu;
        wrapper_arg_for_obu.msg_p = msg;
        wrapper_arg_for_obu.app_section_p = &app_section;
        callback_parameter_pointer = &wrapper_arg_for_obu;
    #else
        /* * 依據老師的 idea，在未來，struct: V2R_app_section_t
        * 可能不會直接傳出去給外部 app
        * 因此多了定義了這個 struct: V2R_self_defined_section_t
        * 用來傳給外部 APP
        * In addition, to handle j2735 decoding issue,
        * we will send msg->msg and msg->msg_len to external-library,
        * the library will decode the msg and complete the V2R_self_defined_section_t
        * at the external client side.
        * */
        V2R_self_defined_section_t V2R_self_defined_section;
        fill_V2R_self_defined_section(&V2R_self_defined_section, object, &app_section);
        V2R_self_defined_section.data = msg->msg;
        V2R_self_defined_section.data_len = msg->msg_len;
        callback_parameter_pointer = &V2R_self_defined_section;
    #endif

    /* since now dispatcher, ea_app_proxy, command_buf_send(),
    * all might read/write callback_list, we add a mutex_lock */
    event_callback_t *current = &callback_list[EVENT_OBU_PACKET_RX];
    // pthread_mutex_lock(&mutex_callback_list);
    while (current->next != NULL) {
        proxy_handling_app_p = current->next->app_obj_p;

        if (current->next->event_callback_id.choice == event_callback_id_msg_id
            && msgf->messageId == current->next->event_callback_id.u.msg_id)
        {
            if(proxy_handling_app_p->ea_info_p){
                current->next->callback( callback_parameter_pointer );
            }
            else{
                current->next->callback( (void *)&app_section ); /* original internal APPs */
            }
        }
        current = current->next;
    }
    // pthread_mutex_unlock(&mutex_callback_list);

    // free resource
    if (app_section.OBU_object != NULL)
        free(app_section.OBU_object);
    if (msgf != NULL)
        J2735_FREE_MSG_FRAME(msgf);

    return PACKET_PROCESSING_ACCEPT;
}

double Smart_AVI_packet_rx_event_handler(msg_obj_t *msg)
{
    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(msg->msg_len);
    if (read_buf.content == NULL) {
        // LOG_MSG_FATAL("OBU_packet_rx_event_handler: malloc");
        perror("Smart_AVI_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        memcpy(read_buf.content, msg->msg, msg->msg_len);
    }
    ObstacleList *obstaclelist = (ObstacleList *) malloc(sizeof(ObstacleList));
    int32_t hour, min;
    float second;

    read_uint32_t(&obstaclelist->device_num, &read_buf);
    read_uint32_t(&obstaclelist->dirct, &read_buf);
    // printf("direct: %d\n", obstaclelist->direct);
    read_uint32_t(&hour, &read_buf);
    // printf("hour: %d\n", hour);
    read_uint32_t(&min, &read_buf);
    // printf("min: %d\n", min);
    read_float(&second, &read_buf);
    // printf("second: %f\n", second);
    read_uint32_t(&obstaclelist->count, &read_buf);
    // printf("count: %d\n", obstaclelist->count);

    // if (obstaclelist->count <= 0)
    //     return 0;

    obstaclelist->tab =
        (Obstacle *) calloc(sizeof(Obstacle), obstaclelist->count);

    if (obstaclelist->tab == NULL) {
        perror("Smart_AVI_packet_rx_event_handler: malloc");
        exit(errno);
    }
    // printf("obstaclelist->count: %d\n", obstaclelist->count);
    for (int i = 0; i < obstaclelist->count; i++) {
        read_double(&obstaclelist->tab[i].lat, &read_buf);
        read_double(&obstaclelist->tab[i].Long, &read_buf);
        read_double(&obstaclelist->tab[i].elev, &read_buf);

        read_uint32_t(&obstaclelist->tab[i].laneID, &read_buf);
        read_uint32_t(&obstaclelist->tab[i].ObstacleID, &read_buf);
        read_uint32_t(&obstaclelist->tab[i].description, &read_buf);

        read_float(&obstaclelist->tab[i].length, &read_buf);
        read_float(&obstaclelist->tab[i].width, &read_buf);

        obstaclelist->tab[i].hour = hour;
        obstaclelist->tab[i].minute = min;
        obstaclelist->tab[i].second = second;

        // printf("lat: %f\n", obstaclelist->tab[i].lat);
        // printf("Long: %f\n", obstaclelist->tab[i].Long);
        // printf("elev: %f\n", obstaclelist->tab[i].elev);
        // printf("laneID: %d\n", obstaclelist->tab[i].laneID);
        // printf("ObstacleID: %d\n", obstaclelist->tab[i].ObstacleID);
        // printf("description: %d\n", obstaclelist->tab[i].description);
        // printf("length: %f\n", obstaclelist->tab[i].length);
        // printf("width: %f\n", obstaclelist->tab[i].width);
        // printf("\r\n");
        read_buf.index += 16;
        read_uint32_t(&obstaclelist->tab[i].speed, &read_buf);
    }
    // printf("\r\n\r\n");

    event_callback_t *current = &callback_list[EVENT_CAMERA_PACKET_RX];
    while (current->next != NULL) {
        if (current->next->event_callback_id.choice == event_callback_id_app_id) {
            current->next->callback((void *) obstaclelist);
        }
        current = current->next;
    }

    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    if (obstaclelist->tab != NULL) {
        free(obstaclelist->tab);
    }
    if (obstaclelist != NULL) {
        free(obstaclelist);
    }
    return 0;
}
//接收smart AVI回傳的封包
// double Smart_AVI_packet_rx_event_handler(msg_obj_t *msg)
// {
//     int cnt = 0;
//     msg_buf_t read_buf;
//     read_buf.index = 0;
//     read_buf.content = (unsigned char *) malloc(25600);
//     if (read_buf.content == NULL) {
//         // log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
//         perror("Smart_AVI_packet_rx_event_handler: malloc");
//         exit(errno);
//     } else {
//         memcpy(read_buf.content, msg->msg, msg->msg_len);
//     }
//     ObstacleList *obstaclelist = (ObstacleList *) malloc(sizeof(ObstacleList));
//     int32_t hour, min;
//     float second;

//     read_uint32_t(&obstaclelist->device_num, &read_buf);
//     read_uint32_t(&obstaclelist->dirct, &read_buf);
//     read_uint32_t(&hour, &read_buf);
//     read_uint32_t(&min, &read_buf);
//     read_float(&second, &read_buf);
//     read_uint32_t(&obstaclelist->count, &read_buf);

//     if(obstaclelist->count <= 0)
//         return 0;

//     obstaclelist->tab =
//         (Obstacle *) calloc(sizeof(Obstacle), obstaclelist->count);

//     if (obstaclelist->tab == NULL) {
//         perror("Smart_AVI_packet_rx_event_handler: malloc");
//         exit(errno);
//     }
//     for (int i = 0; i < obstaclelist->count; i++) {
//         read_double(&obstaclelist->tab[i].lat, &read_buf);
//         read_double(&obstaclelist->tab[i].Long, &read_buf);
//         read_double(&obstaclelist->tab[i].elev, &read_buf);

//         read_uint32_t(&obstaclelist->tab[i].laneID, &read_buf);
//         read_uint32_t(&obstaclelist->tab[i].ObstacleID, &read_buf);
//         read_uint32_t(&obstaclelist->tab[i].description, &read_buf);

//         read_float(&obstaclelist->tab[i].length, &read_buf);
//         read_float(&obstaclelist->tab[i].width, &read_buf);

//         obstaclelist->tab[i].hour = hour;
//         obstaclelist->tab[i].minute = min;
//         obstaclelist->tab[i].second = second;

//         read_buf.index += 16;
//     }

//     /* since now dispatcher, ea_app_proxy, command_buf_send(), 
//     * all might read/write callback_list, we add a mutex_lock */
//     // pthread_mutex_lock(&mutex_callback_list);

//     event_callback_t *current = &callback_list[EVENT_CAMERA_PACKET_RX];
//     while (current->next != NULL) {
//         if (current->next->event_callback_id.choice == event_callback_id_app_id && 
//             CPS_ID == current->next->event_callback_id.u.app_id) {
//         // if (current->next->event_callback_id.choice == event_callback_id_app_id){
//             // if(threadpool_add(pool, current->next->callback, obstaclelist, 0)
//             // != 0){
//             //     printf("threadpool adding error!\n");//ERROR
//             // }
//             proxy_handling_app_p = current->next->app_obj_p;
//             current->next->callback((void *) obstaclelist);
//         }
//         current = current->next;
//     }

//     // pthread_mutex_unlock(&mutex_callback_list);

//     if (read_buf.content != NULL) {
//         free(read_buf.content);
//     }
//     return 0;
// }
// 確認是否接到OBU方heartbeat
int Is_Heartbeat(msg_obj_t *msg)
{
    //接收OBU封包
    msg_buf_t read_buf;
    V2R_common_field_t common_field;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(V2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        LOG_MSG_FATAL("OBU_packet_rx_event_handler: malloc");
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

    //檢查天線
    uint8_t antenna_status;
    read_uint8_t(&antenna_status, &read_buf);
    switch (antenna_status) {
    case 0:
        LOG_MSG_TRACE("No status. Cannot get status from GNSS hardware.");
        break;
    case 1:
        LOG_MSG_TRACE("The GNSS antenna is connected well.");
        break;
    case 2:
        LOG_MSG_TRACE("The connection status of GNSS antenna is open.");
        break;
    case 3:
        LOG_MSG_TRACE("The connection status of GNSS antenna is short.");
        break;
    default:
        break;
    }
    //接收DSRC格式heartbeat封包
    if (common_field.service_id == 0 &&
        common_field.packet_len == 512) {  // dsrc heart beat packet
        LOG_MSG_INFO("dsrc alive and postpone the timer handle execution");
        set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
        clear_dsrc_error();
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

