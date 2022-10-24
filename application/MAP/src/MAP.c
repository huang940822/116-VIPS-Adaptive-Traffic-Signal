#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "MAP.h"
#include "MAP_config.h"
#include "MAP_utils.h"
#include "MAP_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "config.h"
#include "j2735_timer_event.h"
#include "log.h"
#include "timer_event.h"

MapData *map;
timer_t MAP_packet_tx_timer_id;
uint8_t MAP_packet_tx_num = TIMER_EVENT_MAP_PACKET_TX;

app_obj_t MAP = {
    .name = "MAP",
    .id = 6,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &MAP_on_CLOUD_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &MAP_on_registration,
    .dontSend2TC = 1,
    .next = NULL,
};

int MAP_on_CLOUD_packet_rx(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("MAP_on_cloud_packet_rx: malloc");
        perror("MAP_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }

    // needs a map sned ack function to send ack to cloud
    MAP_send_ack();

    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "MAP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "MAP cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            MAP.dontSend2TC == 0) {  // enable/clear command buffer
            MAP.dontSend2TC = 1;
            log_file_write("MAP disable\r\n");
            printf("MAP disable\r\n");
        } else if (enableOrdisable == 2 &&
                   MAP.dontSend2TC == 1) {  
            MAP.dontSend2TC = 0;
            log_file_write("MAP enable and command buffer clear\r\n");
            printf("MAP enable\r\n");
        } else {
            log_file_write(
                "invalid cloud pcket disable/enable packet to tc machine\r\n");
        }
        
    } break;
    default:
        break;
    }

    log_file_write(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

int MAP_on_registration(void *arg)
{
    /* read map confile file*/
    int ret = MAP_config_init();
    if(ret != 0) {
        log_file_write_fatal_error("error map reading config file: %d", ret);
    }
    /* MAP msg init */
    map_msg_init(&map);
    MAP.dontSend2TC = MAP_config.MAP_dontSend2TC;
    /* create a timer to send map packet */
    create_timer(&MAP_packet_tx_timer_id, &MAP_packet_tx_num, j2735_timer_event_handler);
    if(MAP_config.MAP_packet_transfer_speed == 1)
        set_timer(MAP_packet_tx_timer_id, MAP_config.MAP_packet_transfer_speed, 0, 1, 0);
    else
        set_timer(MAP_packet_tx_timer_id, 0, 1000000000 / MAP_config.MAP_packet_transfer_speed, 1, 0);

}