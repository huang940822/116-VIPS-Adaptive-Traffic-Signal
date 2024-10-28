#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "EVSP_packet_tx.h"
#include "EVSP_touching_area.h"

#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

void EVSP_send_ack() //緊急交通工具方傳送ACK訊息
{
    msg_buf_t write_buf;
    write_buf.index = 0;

    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "EVSP_send_ack");

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);
    free(write_buf.content);
    return;
}

// 轉傳緊急封包到雲端
void EVSP_report_host_obu(OBU_object_t *OBU_object, uint8_t on_duty_flag)
{  // it's for evsp service's rx and try to get it's duty status and route
    // it to cloud

    // back of queue;
    uint8_t last_record_index = OBU_object->record_ring.last_record_pointer;

    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(42);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("OBU_packet_tx: malloc");
        perror("OBU_packet_tx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        // clear mem content which is malloced
        memset(write_buf.content, 0, 42);
    }

    //以下開始填入封包訊息

    // write cmd
    write_uint8_t(0, &write_buf);
    // write OBU_name
    write_char(OBU_object->OBU_name, &write_buf, OBU_NAME_MAX_LEN, OBU_NAME_MAX_LEN);
    // write vehicle_type
    write_uint8_t(OBU_object->vehicle_type, &write_buf);

    char timestamp_t[20];
    struct tm timeinfo;
    localtime_r(&OBU_object->record_ring.record[last_record_index].time_second, &timeinfo);
    // write timestamp
    int length = strftime(timestamp_t, 20, "%Y-%m-%d %H:%M:%S\n", &timeinfo);
    write_char(timestamp_t, &write_buf, TIMESTAMP_LEN, TIMESTAMP_LEN);

    // write lon
    write_float(OBU_object->record_ring.record[last_record_index].position_lon, &write_buf);
    // write lat
    write_float(OBU_object->record_ring.record[last_record_index].position_lat, &write_buf);

    // write speed
    write_uint8_t(OBU_object->record_ring.record[last_record_index].speed, &write_buf);
    // write direction
    write_uint8_t(OBU_object->record_ring.record[last_record_index].direction, &write_buf);

    // write on_duty_flag
    write_uint8_t(on_duty_flag, &write_buf);

    cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);
    free(write_buf.content);
}


void EVSP_report_activate_area(OBU_object_t *OBU_object, area_type_t type, int areaId)
{
    msg_buf_t write_buf;
    write_buf.index = 0;

    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "EVSP_send_ack");

    // cmd
    write_uint8_t(1, &write_buf);

    // write OBU_name
    write_char(OBU_object->OBU_name, &write_buf, OBU_NAME_MAX_LEN, OBU_NAME_MAX_LEN);
    // write vehicle_type
    write_uint8_t(OBU_object->vehicle_type, &write_buf);

    // area type
    write_uint8_t(type, &write_buf);
    // area id
    write_uint32_t(areaId, &write_buf);

    // back of queue;
    uint8_t last_record_index = OBU_object->record_ring.last_record_pointer;
    // write lon
    write_float(OBU_object->record_ring.record[last_record_index].position_lon, &write_buf);
    // write lat
    write_float(OBU_object->record_ring.record[last_record_index].position_lat, &write_buf);

    // write speed
    write_uint8_t(OBU_object->record_ring.record[last_record_index].speed, &write_buf);
    // write direction
    write_uint8_t(OBU_object->record_ring.record[last_record_index].direction, &write_buf);

    log_file_write("report cloud area obu name %s type %d area id %d", OBU_object->OBU_name, type, areaId);
    
    cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);

    free(write_buf.content);
}