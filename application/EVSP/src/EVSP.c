#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include "log.h"
#include "EVSP.h"
#include "config.h"
#include "error_status.h"
#include "timer_event.h"
#include "EVSP_OBU_list.h"
#include "byte_processing.h"
#include "gps_information.h"
#include "EVSP_typedefine.h"
#include "EVSP_timer_event.h"
#include "EVSP_touching_area.h"
#include "EVSP_packet_tx.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "com_packet_processing.h"
#include "EVSP_config.h"

app_obj_t EVSP = {
    .name = "EVSP",
    .id = 1,
    .priority = 1,
    .on_OBU_packet_rx = &EVSP_on_OBU_packet_rx,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &EVSP_on_CLOUD_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &EVSP_on_registration,
    .next = NULL,
    .dontSend2TC=1,
};


int EVSP_on_CLOUD_packet_rx(void *arg){
    // printf("\nTSP_on_cloud_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    // typedef struct C2R_app_section {
    //     uint32_t payload_len;
    //     char *payload;
    //     uint8_t com_id;
    // } C2R_app_section_t;
    C2R_app_section_t *app_section = (C2R_app_section_t *)arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned  char *)malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_on_cloud_packet_rx: malloc");
        perror("TSP_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload, app_section->payload_len);
    }

    // needs a evsp sned ack function to send ack to cloud
    EVSP_send_ack();
    
    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP cloud packet rx: CMD(%d)", cmd);

    switch(cmd){
        case 0:
        {   //disable/enalbe:1/2
            uint8_t enableOrdisable=0;
            // uint8_t type=0;
            read_int8_t(&enableOrdisable, &read_buf);
            // read_int8_t(&type, &read_buf);
            if(enableOrdisable==1&& EVSP.dontSend2TC==0){  //enable/clear command buffer
                EVSP.dontSend2TC= 1;
                log_file_write("evsp disable\r\n");
                printf("evsp disable\r\n");
            }else if(enableOrdisable==2 && EVSP.dontSend2TC==1){   //disable command buffer/then stop the command in command buffer sent to tc machine
                EVSP.dontSend2TC= 0;
                command_buf_clear();
                log_file_write("evsp enable and command buffer clear\r\n");    
                printf("evsp enable\r\n");
            }else{
                log_file_write("invalid cloud pcket disable/enable packet to tc machine\r\n");
            }
            // printf("not implement evsp on cloud rx action yet when cmd is 0\r\n");
        }
        break;
        default:
        break;
    }

    log_file_write(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;

}

//讀取到evsp obu的封包時

// typedef struct V2R_app_section {
//     uint32_t payload_len;
//     char *payload;
//     uint8_t com_id;
//     OBU_object_t *OBU_object;
// } V2R_app_section_t;

// typedef struct OBU_object {
//     char OBU_id[OBU_ID_MAX_LEN + 1];    //+1 if for \0
//     uint8_t vehicle_type;
//     OBU_record_ring_t record_ring;
//     app_private_space_t *private_space;
//     struct OBU_object *prev;
//     struct OBU_object *next;
// } OBU_object_t;

// typedef struct OBU_record_ring {
//     OBU_record_t record[OBU_RECORD_RING_CAPACITY];
//     uint8_t first_record_pointer; // queue.front
//     uint8_t last_record_pointer; // queue.back
//     uint8_t length;
// } OBU_record_ring_t;

// typedef struct OBU_record {
//     char OBU_id[OBU_ID_MAX_LEN];
//     struct tm time_stamp;
//     time_t time_second;
//     float position_lon;
//     float position_lat;
//     uint8_t speed;
//     uint8_t acceleration;
//     uint8_t direction;
//     uint8_t vehicle_type;
// } OBU_record_t;
//int length= strftime (buffer,80,"%Y-%m-%dT%H:%M:%SZ\n",timeinfo); 
//可用此函式把tm結構timestamp
//變回去string

int EVSP_on_OBU_packet_rx(void *arg)
{
    // printf("EVSP_on_OBU_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    V2R_app_section_t *app_section = (V2R_app_section_t *)arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *)malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("EVSP_on_OBU_packet_rx: malloc");
        perror("EVSP_on_OBU_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload, app_section->payload_len);
    }
    
    /* print packet */
    if (config.log_OBU_packet_rx) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP OBU packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    // typedef struct EVSP_static_space
    // {
    //     uint8_t on_duty_flag;
    //     uint8_t weight;
    //     uint8_t last_direction;
    //     float last_lon;
    //     float last_lat;
    // } EVSP_static_space_t;
    EVSP_static_space_t static_space;
    memcpy(&static_space, app_section->OBU_object->private_space[EVSP.id].static_space, sizeof(EVSP_static_space_t));
    read_uint8_t(&static_space.on_duty_flag, &read_buf);
    read_uint8_t(&static_space.weight, &read_buf);



    uint8_t last_record_index = app_section->OBU_object->record_ring.last_record_pointer;   //back of queue; 最新推入的資料？

    //轉傳緊急封包到雲端

    
    { //it's for evsp service's rx and try to get it's duty status and route it to cloud
        // printf("recv special obu object and will route it to cloud for it's evsp packet\n\r");
        
        msg_buf_t write_buf;
        write_buf.index = 0;
        write_buf.content = (unsigned char *)malloc(42);
        if (write_buf.content == NULL) {
            set_memory_error();
            log_file_write_fatal_error("OBU_packet_tx: malloc");
            perror("OBU_packet_tx: malloc");
            exit(errno);
        } else {
            clear_memory_error();
            //clear mem content which is malloced
            memset(write_buf.content, 0, 42);
        }
        write_uint8_t(0, &write_buf); //write cmd
        write_char(app_section->OBU_object->record_ring.record[last_record_index].OBU_id, &write_buf, OBU_ID_MAX_LEN, OBU_ID_MAX_LEN);
        write_uint8_t(app_section->OBU_object->record_ring.record[last_record_index].vehicle_type, &write_buf);
        
        char timestamp_t[20];
        int length= strftime (timestamp_t,20,"%Y-%m-%d %H:%M:%S\n",(struct tm *)&(app_section->OBU_object->record_ring.record[last_record_index].time_stamp));
        // printf("obu object timestamp string is %s\n\r",timestamp_t);
        write_char(timestamp_t, &write_buf, TIMESTAMP_LEN, TIMESTAMP_LEN);
        write_float(app_section->OBU_object->record_ring.record[last_record_index].position_lon, &write_buf);
        write_float(app_section->OBU_object->record_ring.record[last_record_index].position_lat, &write_buf);

        //printf("gps data %f %f\r\n",app_section->OBU_object->record_ring.record[last_record_index].position_lon, 
        //app_section->OBU_object->record_ring.record[last_record_index].position_lat);

        write_uint8_t(app_section->OBU_object->record_ring.record[last_record_index].speed, &write_buf);
        write_uint8_t(app_section->OBU_object->record_ring.record[last_record_index].direction, &write_buf);
        
        //write dummy duty
        write_uint8_t(1, &write_buf);
        printf("route evsp to cloud\r\n");
        cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);
        free(write_buf.content);
    }


    //obu與rsu的距離
    uint16_t OBU_distance = (uint16_t)get_distance(config.RSU_lat, config.RSU_lon, 
                                app_section->OBU_object->record_ring.record[last_record_index].position_lat, 
                                app_section->OBU_object->record_ring.record[last_record_index].position_lon);


    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP OBU packet rx: OBU POSITION");
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), 
        "\nlat, lon: %f, %f\nOBU distance: %hd\nOBU direction: %hhd", 
        app_section->OBU_object->record_ring.record[last_record_index].position_lat, 
        app_section->OBU_object->record_ring.record[last_record_index].position_lon,
        OBU_distance, 
        app_section->OBU_object->record_ring.record[last_record_index].direction);
    
    uint16_t last_record_distance = 0;

    //有舊資料
    if (static_space.last_lon != 0 && static_space.last_lat != 0) {
        last_record_distance = (uint16_t)get_distance(static_space.last_lat, static_space.last_lon, 
                                app_section->OBU_object->record_ring.record[last_record_index].position_lat, 
                                app_section->OBU_object->record_ring.record[last_record_index].position_lon);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), 
            "\nlast lat, last lon: %f, %f\nlast record distance: %hd\nlast OBU direction: %hhd", 
            static_space.last_lat, 
            static_space.last_lon,
            last_record_distance, 
            static_space.last_direction);
        
        if (last_record_distance > EVSP_config.valid_record_distance) { //位移有超過閥值 才會紀錄下來？
            static_space.last_lon = app_section->OBU_object->record_ring.record[last_record_index].position_lon;
            static_space.last_lat = app_section->OBU_object->record_ring.record[last_record_index].position_lat;
            static_space.last_direction = app_section->OBU_object->record_ring.record[last_record_index].direction;
        }
    } else {    //第一筆資料
        static_space.last_lon = app_section->OBU_object->record_ring.record[last_record_index].position_lon;
        static_space.last_lat = app_section->OBU_object->record_ring.record[last_record_index].position_lat;
        static_space.last_direction = app_section->OBU_object->record_ring.record[last_record_index].direction;
    }
    memcpy(app_section->OBU_object->private_space[EVSP.id].static_space, &static_space, sizeof(EVSP_static_space_t));
    log_file_write(log_content);

    EVSP_host_OBU_obj_t *host_OBU = EVSP_host_OBU_obj_search(app_section->OBU_object->OBU_id);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        // printf("SubPhaseID is 0\n");
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return 0;
    }//tc箱出現錯誤 直接不做

    memset(log_content, 0, sizeof(log_content));
    /* already in host OBU list */
    if (host_OBU != NULL) { 
        set_timer(host_OBU->host_OBU_packet_timer, 0, 0, EVSP_config.evsp_host_obu_packet_timeout, 0);
        host_OBU->distance = OBU_distance;
        uint8_t terminate = EVSP_terminate(app_section->OBU_object->record_ring.record[last_record_index].position_lon,
                                            app_section->OBU_object->record_ring.record[last_record_index].position_lat,
                                            host_OBU->area_ptr);
        int ret = 0;
        // enter terminate area
        if (terminate == true) {

            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP OBU packet rx: TERMINATE");
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nOBU ID: %s", app_section->OBU_object->OBU_id);

            tsc_command_t command;
            memset(&command, 0, sizeof(tsc_command_t));
            command.app_id = EVSP.id;
            command.app_priority = EVSP.priority;
            command.target_phase = host_OBU->target_phase;
            strncpy(command.host_OBU_id, RESUME_ID, OBU_ID_MAX_LEN);

            EVSP_host_OBU_obj_delete(app_section->OBU_object->OBU_id);

            // no other host OBU with same target phase in host_OBU_list
            if (EVSP_host_OBU_obj_resume(command.target_phase) == true) {
                uint8_t current_phase = signal_status.SubPhaseID;
                if (command.target_phase >= current_phase) {    
                    command.cycle = 0;
                    command.phase = command.target_phase;
                    command.effect_time = signal_status.plan[command.target_phase - 1].PreTimeCompensated;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                } else {    //target phase已過 到下一個cycle執行
                    command.cycle = 1;
                    command.phase = command.target_phase;
                    command.effect_time = signal_status.plan[command.target_phase - 1].PreTimeCompensated;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
            }
        }

    } else { /* not in host OBU list */
        // search plan
        uint8_t plan_id = get_plan_id();
        EVSP_touching_area_plan_list_t *plan = EVSP_touching_area_plan_search(plan_id);

        if (plan == NULL) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ntouching area plan not found");
            log_file_write(log_content);
            if (read_buf.content != NULL) {
                free(read_buf.content);
            }
            return 0;
        }

        uint8_t target_phase = 0;
        EVSP_touching_area_t *area_ptr = EVSP_activate(app_section->OBU_object->record_ring.record[last_record_index].position_lon,
                                            app_section->OBU_object->record_ring.record[last_record_index].position_lat,
                                            static_space.last_direction,
                                            &target_phase, plan);
        // enter activate area
        if (target_phase >= 0 && target_phase < EVSP_PHASE_MAX) {
            target_phase += 1;  //why +1  ??因為phase的值會在0~7但實際上會是1~8
            // printf("max green is %d\r\n", EVSP_config.max_green);
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "EVSP OBU packet rx: ACTIVATE");
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nOBU ID: %s", app_section->OBU_object->OBU_id);
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ntarget phase: %d", target_phase);
            
            EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_id, target_phase, area_ptr);
            EVSP_host_OBU_obj_print();
            tsc_command_t command;
            memset(&command, 0, sizeof(tsc_command_t));
            command.app_id = EVSP.id;
            command.app_priority = EVSP.priority;
            command.target_phase = target_phase;
            strncpy(command.host_OBU_id, app_section->OBU_object->OBU_id, OBU_ID_MAX_LEN);

            uint8_t current_phase = signal_status.SubPhaseID;
            uint8_t current_step = signal_status.StepID;
            int ret = 0;

            /* target_phase == current_phase */
            if (target_phase == current_phase && current_step == 1) {
                command.cycle = 0;
                command.phase = current_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }
            if (target_phase == current_phase && current_step != 1) {
                for (int i = current_phase + 1; i <= signal_status.SubPhaseCount; i++) {
                    command.cycle = 0;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                for (int i = 1; i < target_phase; i++) {
                    command.cycle = 1;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                command.cycle = 1;
                command.phase = target_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }

            /* target_phase > current_phase */
            if (target_phase > current_phase && current_step == 1) {
                for (int i = current_phase; i < target_phase; i++) {
                    command.cycle = 0;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                command.cycle = 0;
                command.phase = target_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }
            if (target_phase > current_phase && current_step != 1) {
                for (int i = current_phase + 1; i < target_phase; i++) {
                    command.cycle = 0;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                command.cycle = 0;
                command.phase = target_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }

            /* target_phase < current_phase */
            if (target_phase < current_phase && current_step == 1) {
                for (int i = current_phase; i <= signal_status.SubPhaseCount; i++) {
                    command.cycle = 0;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                for (int i = 1; i < target_phase; i++) {
                    command.cycle = 1;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                command.cycle = 1;
                command.phase = target_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }
            if (target_phase < current_phase && current_step != 1) {
                for (int i = current_phase + 1; i <= signal_status.SubPhaseCount; i++) {
                    command.cycle = 0;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                for (int i = 1; i < target_phase; i++) {
                    command.cycle = 1;
                    command.phase = i;
                    command.effect_time = EVSP_config.min_green;
                    ret = command_buf_insert_effect_time(&command);
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                        command.cycle, command.phase, command.effect_time, ret);
                }
                command.cycle = 1;
                command.phase = target_phase;
                command.effect_time = EVSP_config.max_green;
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, effect time: %d (%d)", 
                    command.cycle, command.phase, command.effect_time, ret);
            }
        }
    }
    log_file_write(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

int EVSP_on_registration(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    /* touching area */
    DIR * dp;
    struct dirent * dirp;
    if ((dp = opendir(TOUCHING_AREA_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", TOUCHING_AREA_DIR);
    } else {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s opened successfully", TOUCHING_AREA_DIR);
        log_file_write(log_content);
    }

    char rsu_id[RSU_ID_MAX_LEN];
    uint8_t plan_id;
    /* list all file */
    while ( (dirp = readdir(dp)) != NULL ) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_id, &plan_id);
            if (strncmp(rsu_id, config.RSU_id, RSU_ID_MAX_LEN) == 0) {
                EVSP_touching_area_plan_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);
    EVSP_touching_area_plan_print();
    return 0;
}