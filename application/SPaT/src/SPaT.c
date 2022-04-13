#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "SPaT.h"
#include "SPaT_config.h"
#include "SPaT_utils.h"
#include "SPaT_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "config.h"
#include "j2735_timer_event.h"
#include "log.h"
#include "timer_event.h"

SPAT *p_spat;
timer_t SPaT_packet_tx_timer_id;
uint8_t SPaT_packet_tx_num = TIMER_EVENT_SPAT_PACKET_TX;

app_obj_t SPaT = {
    .name = "SPaT",
    .id = 5,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &SPaT_on_CLOUD_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &SPaT_on_registration,
    .dontSend2TC = 1,
    .next = NULL,
};

int SPaT_on_CLOUD_packet_rx(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("SPaT_on_cloud_packet_rx: malloc");
        perror("SPaT_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }
printf("asd\n");
    // needs a evsp sned ack function to send ack to cloud
    SPaT_send_ack();
printf("asdaa\n");
    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "SPaT cloud packet rx: SPECIFIC FIELD\n");
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
             "SPaT cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            SPaT.dontSend2TC == 0) {  // enable/clear command buffer
            SPaT.dontSend2TC = 1;
            log_file_write("SPaT disable\r\n");
            printf("SPaT disable\r\n");
        } else if (enableOrdisable == 2 &&
                   SPaT.dontSend2TC == 1) {  
            SPaT.dontSend2TC = 0;
            log_file_write("SPaT enable and command buffer clear\r\n");
            printf("SPaT enable\r\n");
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

int SPaT_on_registration(void *arg)
{
    /* init spat msg */
    spat_msg_init(&p_spat);

    /* create a timer to send SPaT packet */
    create_timer(&SPaT_packet_tx_timer_id, &SPaT_packet_tx_num, j2735_timer_event_handler);
    set_timer(SPaT_packet_tx_timer_id, 0, 1000000000 / SPaT_config.SPaT_packet_transfer_speed, 1, 0);
}