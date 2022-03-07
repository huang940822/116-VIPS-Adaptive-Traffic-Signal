#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include "SPaT.h"
#include "SPaT_utils.h"
#include "SPaT_config.h"
#include "log.h"
#include "config.h"
#include "timer_event.h"
#include "j2735_timer_event.h"

SPAT *p_spat;
timer_t SPaT_packet_tx_timer_id;
uint8_t SPaT_packet_tx_num = TIMER_EVENT_SPAT_PACKET_TX;

app_obj_t SPaT = {
    .name = "SPaT",
    .id = 9,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &SPaT_on_registration,
    .next = NULL,
};


int SPaT_on_registration(void *arg)
{
    /* init spat msg */
    spat_msg_init(&p_spat);
    
    /* create a timer to send SPaT packet */
    if(config.SPaT_packet_tx){
        create_timer(&SPaT_packet_tx_timer_id,&SPaT_packet_tx_num,j2735_timer_event_handler);
        set_timer(SPaT_packet_tx_timer_id, 0, 1000000000/SPaT_config.SPaT_packet_transfer_speed , 1, 0);
    }
}   