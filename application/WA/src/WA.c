#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "WA.h"
#include "WA_config.h"
#include "WA_timer_event.h"
#include "ObstacleList.h"
#include "buffer.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "log.h"
#include "post_processing.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"


app_obj_t WA = {
    .name = "WA",
    .id = WA_ID,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = WA_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = WA_on_registration,
    .next = NULL,
};

int WA_on_camera_packet_rx(void *arg){
    ObstacleList *obstaclelist = (ObstacleList *) arg;
    for(int i = 0; i < obstaclelist->count; i++) {
        int ObstacleID = obstaclelist->tab[i].ObstacleID;
        double latitude = obstaclelist->tab[i].lat;
        double longitude = obstaclelist->tab[i].Long;
        log_file_write("obstacle: %d lat: %lf long: %lf \r\n", ObstacleID, latitude, longitude);
    }
}

int WA_on_registration(void *arg){
    int ret = 0;
    log_file_write("WA SAY HELLO");
}


