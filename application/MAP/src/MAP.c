#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "MAP.h"
#include "MAP_config.h"
#include "config.h"
#include "j2735_timer_event.h"
#include "log.h"
#include "timer_event.h"


timer_t MAP_packet_tx_timer_id;
uint8_t MAP_packet_tx_num = TIMER_EVENT_MAP_PACKET_TX;

app_obj_t MAP = {
    .name = "MAP",
    .id = 8,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &MAP_on_registration,
    .next = NULL,
};


int MAP_on_registration(void *arg)
{
    /* init map msg */
    MAP_config_init();
    /* create a timer to send map packet */
    if (config.MAP_packet_tx) {
        create_timer(&MAP_packet_tx_timer_id, &MAP_packet_tx_num, j2735_timer_event_handler);
        set_timer(MAP_packet_tx_timer_id, 0, 1000000000 / MAP_config.MAP_packet_transfer_speed, 1, 0);
    }
}