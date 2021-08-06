#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "log.h"
#include "com_io.h"
#include "config.h"
#include "server.h"
#include "msg_queue.h"
#include "dispatcher.h"
#include "typedefine.h"
#include "error_status.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "OBU_record_processing.h"
#include "application_registration.h"
#include "traffic_signal_packet_rx.h"
#include "timer_event.h"

void OBU_packet_tx(uint16_t len, uint8_t service_id, unsigned char *specific_field)
{
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *)malloc(R2V_COMMON_FIELD_LEN + len);
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
    write_char(config.RSU_id, &write_buf, sizeof(config.RSU_id) - 1, RSU_ID_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[20];
    time(&rawtime);
    info = localtime( &rawtime );
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
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "OBU packet tx: service(%d) length(%d)\n", service_id, write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", write_buf.content[i]);
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

//to send packet to cloud
void cloud_packet_tx(uint16_t len, uint8_t service_id, unsigned char *specific_field)
{
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *)malloc(R2C_COMMON_FIELD_LEN + len);
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
    write_char(config.RSU_id, &write_buf, sizeof(config.RSU_id) - 1, RSU_ID_MAX_LEN);

    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[20];
    time(&rawtime);
    info = localtime( &rawtime );
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
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "cloud packet tx: service(%d) length(%d)\n", service_id, write_buf.index);
        for (int i = 0; i < write_buf.index; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", write_buf.content[i]);
        }
        log_file_write(log_content);
    }
    
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "in cloud packet tx cloud_com_id is %d\n", cloud_com_id);
    log_file_write(log_content);
    int ret = com_send(cloud_com_id, write_buf.content, write_buf.index);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("cloud_packet_tx: com_send");
    }

    usleep(50000);   //直接註解com layer會錯

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
    read_buf.content = (unsigned char *)malloc(C2R_COMMON_FIELD_LEN);
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
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "cloud packet rx: COMMON FIELD\n");
        for (int i = 0; i < read_buf.index; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", read_buf.content[i]);
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
    if (common_field.device_type < 0 || common_field.device_type >= DEVICE_TYPE_NUMBER) {
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
    app_section.payload = (char *)malloc(app_section.payload_len);
    if (app_section.payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("cloud_packet_rx_event_handler: malloc");
        perror("cloud_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.payload, &msg->msg[read_buf.index], app_section.payload_len);
        app_section.com_id = msg->handle_id;
    }

    event_callback_t *current = &callback_list[EVENT_CLOUD_PACKET_RX];
    while (current->next != NULL) {
        
        if (common_field.service_id == current->next->app_id) {
                
            current->next->callback((void *)&app_section);  //what com_id for?
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

int OBU_packet_rx_event_handler(msg_obj_t *msg)
{   printf("get in obu rx handler\n\r");
    char log_content[LOG_CONTENT_LEN + 1];

    msg_buf_t read_buf;
    V2R_common_field_t common_field;
    read_buf.index = 0;
    read_buf.content = (unsigned char *)malloc(V2R_COMMON_FIELD_LEN);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, msg->msg, V2R_COMMON_FIELD_LEN);
    }

    read_buf.index = V2R_COMMON_FIELD_LEN;

    if (config.log_OBU_packet_rx) {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "OBU packet rx: COMMON FIELD\n");
        for (int i = 0; i < read_buf.index; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", read_buf.content[i]);
        }
        log_file_write(log_content);
    }

    read_buf.index = 0;

    // packet_len 
    read_uint32_t(&common_field.packet_len, &read_buf);
    // device type
    read_uint8_t(&common_field.device_type, &read_buf);
    // OBU id
    read_char(common_field.OBU_id, &read_buf, OBU_ID_MAX_LEN);
    // vehicle_type
    read_uint8_t(&common_field.vehicle_type, &read_buf);
    // timestamp
    read_char(common_field.timestamp, &read_buf, TIMESTAMP_LEN);
    // position
    read_float(&common_field.position_lon, &read_buf);
    read_float(&common_field.position_lat, &read_buf);
    // speed
    read_uint8_t(&common_field.speed, &read_buf);
    // direction
    read_uint8_t(&common_field.direction, &read_buf);
    // service id
    read_uint8_t(&common_field.service_id, &read_buf);

    /* common field value valid */
    if (common_field.packet_len < V2R_COMMON_FIELD_LEN) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_PACKET_LEN;
    }
    if (common_field.device_type < 0 || common_field.device_type >= DEVICE_TYPE_NUMBER) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_DEVICE_TYPE;
    }
    if (common_field.vehicle_type < 0 || common_field.vehicle_type >= VEHICLE_TYPE_NUMBER ) {
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_VEHICLE_TYPE;
    }
    if (common_field.service_id < 0) {  //0 is for middleware
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return PACKET_INVALID_SEVICE_ID;
    }
    
    // 這裡是用來偵測dsrc是否還活著
    if(common_field.service_id==0){ //dsrc heart beat packet
        printf("dsrc alive and postpone the timer handle execution\r\n");
        log_file_write("dsrc alive and postpone the timer handle execution\r\n");
        set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
        clear_dsrc_error();

        return PACKET_PROCESSING_ACCEPT;    
    }


    OBU_record_t *record = (OBU_record_t *)malloc(sizeof(OBU_record_t));
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

    

    //record和obu object都有obu id這樣才知道要把packet裡面節錄出來的record資料
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
    app_section.payload_len = common_field.packet_len - V2R_COMMON_FIELD_LEN;

    app_section.payload = (char *)malloc(app_section.payload_len);
    if (app_section.payload == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_rx_event_handler: malloc");
        perror("OBU_packet_rx_event_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(app_section.payload, &msg->msg[read_buf.index], app_section.payload_len);
        app_section.com_id = msg->handle_id;
    }  

    app_section.OBU_object = (OBU_object_t *)malloc(sizeof(OBU_object_t));
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
            
            current->next->callback((void *)&app_section);
        }
        current = current->next;
    }


    //free resource just
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    if (app_section.payload != NULL) {
        free(app_section.payload);
    }
    if (app_section.OBU_object != NULL) {
        free(app_section.OBU_object);
    }
    
    return PACKET_PROCESSING_ACCEPT;
}