#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "TIB.h"
#include "TIB_MAP_utils.h"
#include "TIB_SPaT_utils.h"
#include "TIB_config.h"
#include "TIB_packet_tx.h"
#include "TIB_utils.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"

#include "j2735_codec.h"
#include "j2735_map.h"

LOG_USE_MODULE(TIB);

timer_t MAP_packet_tx_timer_id;

app_obj_t TIB = {
    .name = "TIB",
    .id = TIB_ID,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &TIB_on_CLOUD_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_traffic_signal_command_tx = &TIB_on_traffic_signal_command_tx,
    .on_registration = &TIB_on_registration,
    .dontSend2TC = 1,
    .next = NULL,
};

int TIB_on_CLOUD_packet_rx(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        LOG_MSG_FATAL("TIB_on_cloud_packet_rx: malloc");
        perror("TIB_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }

    // needs a map sned ack function to send ack to cloud
    TIB_send_ack();

    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        LOG_MSG_APPEND(log_content, "MAP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            LOG_MSG_APPEND(log_content, "%x ", read_buf.content[i]);
        }
        LOG_MSG_APPEND(log_content, "\n");
    }

    LOG_MSG_APPEND(log_content, "MAP cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            TIB.dontSend2TC == 0) {  // enable/clear command buffer
            TIB.dontSend2TC = 1;
            LOG_MSG_INFO("MAP disable");
        } else if (enableOrdisable == 2 &&
                   TIB.dontSend2TC == 1) {
            TIB.dontSend2TC = 0;
            LOG_MSG_INFO("MAP enable and command buffer clear");
        } else {
            LOG_MSG_INFO(
                "invalid cloud pcket disable/enable packet to tc machine");
        }

    } break;
    default:
        break;
    }

    LOG_MSG_INFO(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

int TIB_on_traffic_signal_command_tx(void *arg)
{
    traffic_signal_command_arg_t *command = (traffic_signal_command_arg_t *) arg;
    traffic_signal_status_t status;

    // if (strncmp(command->host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME) - 1) == 0 ||
    //     strncmp(command->host_OBU_name, RESUME_ID, sizeof(RESUME_ID) - 1) == 0) {
    //     return 0;
    // }

    get_traffic_signal_status(&status);
    set_adjust_time(command->effect_time - status.StepSec);
    return 0;
}

int TIB_on_registration(void *arg)
{
    /* read map confile file*/
    int ret = TIB_config_init();
    if (ret != 0) {
        LOG_MSG_FATAL("error TIB reading config file: %d", ret);
        return 0;
    }
    /* SPaT / MAP msg init */
    map_msg_init(&map);
    spat_msg_init(&p_spat);
    char log_content[LOG_CONTENT_LEN + 1] = {0};
    print_config_map(map, log_content, LOG_CONTENT_LEN);

    pthread_t MAP_packet_tx_thread;
    ret = pthread_create(&MAP_packet_tx_thread, NULL, MAP_packet_tx_loop, NULL);
    pthread_detach(MAP_packet_tx_thread);
    pthread_t SPaT_packet_tx_thread;
    ret = pthread_create(&SPaT_packet_tx_thread, NULL, SPaT_packet_tx_loop, NULL);
    pthread_detach(SPaT_packet_tx_thread);

    return 0;
}