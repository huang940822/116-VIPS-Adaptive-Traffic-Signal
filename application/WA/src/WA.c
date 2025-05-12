#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "WA.h"
#include "WA_config.h"
#include "WA_timer_event.h"
#include "WA_util.h"
#include "ObstacleList.h"
#include "buffer.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "log.h"
#include "util.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
#include "cms.h"

#define NO_VEHICLE -1.0 

LOG_USE_MODULE(WA);

wa_vehicle_t Leading_Vehicles[4]; // Leading Vehicles for four directions NSWE
timer_t WA_Agent_timer_id;
pthread_mutex_t mutex_LV = PTHREAD_MUTEX_INITIALIZER;

void initializationLeadingVehicles(wa_vehicle_t vehicles[], int size){
    for (int i = 0; i < size; i++){
        vehicles[i].speed = NO_VEHICLE;
        vehicles[i].distance = NO_VEHICLE;
    }

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    LOG_MSG_APPEND(log_content, "Initializing Leading Vehicles:");
    for (int i = 0; i < size; i++){
        LOG_MSG_APPEND(log_content,
                 "\n Direction %d: Speed=%.2f, Distance=%.2f",
                 i, vehicles[i].speed, vehicles[i].distance);
    }
    LOG_MSG_TRACE(log_content);
}
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
    .on_camera_packet_rx = &WA_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &WA_on_registration,
    .next = NULL,
};

/* retrieve the speed and calculate the distance between leading vehicles and intersection */
int WA_on_camera_packet_rx(void *arg){ 
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    LOG_MSG_TRACE("WA_on_camera_packet_rx called.");

    ObstacleList *obstaclelist = (ObstacleList *) arg;
    int direct = obstaclelist->dirct;
    double intersection_center_lat = WA_config.intersection_center_lat;
    double intersection_center_lon = WA_config.intersection_center_lon;

    wa_vehicle_t leading_vehicle;
    leading_vehicle.speed = NO_VEHICLE;
    leading_vehicle.distance = NO_VEHICLE;

    for (int i = 0; i < obstaclelist->count; i++) {
        double lat = obstaclelist->tab[i].lat;
        double lon = obstaclelist->tab[i].Long;
        double speed = obstaclelist->tab[i].speed;

        if (!is_valid_gps(lat, lon)) {
            LOG_MSG_WARN("Invalid GPS at index %d: lat=%.6f, lon=%.6f", i, lat, lon);
            continue;
        }

        if (!is_valid_speed(speed)) {
            LOG_MSG_WARN("Invalid speed at index %d: speed=%.2f", i, speed);
            continue;
        }

        double current_distance = distance(lat, lon, intersection_center_lat, intersection_center_lon);
        LOG_MSG_INFO("Incoming vehicle %d: lat=%.6f, lon=%.6f, speed=%.2f, distance=%.2f",
                       i, lat, lon, speed, current_distance);

        if (leading_vehicle.distance == NO_VEHICLE || current_distance < leading_vehicle.distance) {
            leading_vehicle.speed = speed * 0.2778; // km/hr to m/s
            leading_vehicle.distance = current_distance;
        }
    }

    pthread_mutex_lock(&mutex_LV);
    Leading_Vehicles[direct] = leading_vehicle;
    pthread_mutex_unlock(&mutex_LV);

    LOG_MSG_INFO("Direction %d updated: Speed=%.2f, Distance=%.2f",
             direct, leading_vehicle.speed, leading_vehicle.distance);
}

int WA_on_registration(void *arg){
    int ret = 0;
    initializationLeadingVehicles(Leading_Vehicles, 4);

    ret = WA_config_init();
    if(ret != 0){
        LOG_MSG_ERROR("error WA reading config file: %d", ret);
    }
    LOG_MSG_INFO("Regist WA app with frequency: %f", WA_config.warning_freq);
    double freq = WA_config.warning_freq;
    int freq_sec = (int) freq;
    long freq_nsec = (long)((freq - freq_sec) * 1e9);
    create_timer(&WA_Agent_timer_id, NULL, WA_Agent_timer_handler);
    set_timer(WA_Agent_timer_id, freq_sec, freq_nsec, 4, 0);
}


