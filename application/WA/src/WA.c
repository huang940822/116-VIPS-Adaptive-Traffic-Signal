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
#include "post_processing.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
#include "cms.h"

#define NO_VEHICLE -1.0 

CCI Leading_Vehicles[4]; // Leading Vehicles for four directions NSWE
timer_t WA_Agent_timer_id;
pthread_mutex_t mutex_LV = PTHREAD_MUTEX_INITIALIZER;

void initializationLeadingVehicles(CCI vehicles[], int size){
    for (int i = 0; i < size; i++){
        vehicles[i].speed = NO_VEHICLE;
        vehicles[i].distance = NO_VEHICLE;
    }

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    snprintf(log_content, LOG_CONTENT_LEN, "\nInitializing Leading Vehicles:");
    for (int i = 0; i < size; i++){
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\n Direction %d: Speed=%.2f, Distance=%.2f",
                 i, vehicles[i].speed, vehicles[i].distance);
    }
    log_file_write(log_content);
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

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\n WA_on_camera_packet_rx called.");
    log_file_write(log_content);

    ObstacleList *obstaclelist = (ObstacleList *) arg;
    int direct = obstaclelist->dirct;
    double intersection_center_lat = WA_config.intersection_center_lat;
    double intersection_center_lon = WA_config.intersection_center_lon;

    CCI leading_vehicle;
    leading_vehicle.speed = NO_VEHICLE;
    leading_vehicle.distance = NO_VEHICLE;

    for (int i = 0; i < obstaclelist->count; i++) {
        double lat = obstaclelist->tab[i].lat;
        double lon = obstaclelist->tab[i].Long;
        double speed = obstaclelist->tab[i].speed;

        if (!is_valid_gps(lat, lon)) {
            snprintf(log_content, LOG_CONTENT_LEN,
                     "[WARN] Invalid GPS at index %d: lat=%.6f, lon=%.6f", i, lat, lon);
            log_file_write(log_content);
            continue;
        }

        if (!is_valid_speed(speed)) {
            snprintf(log_content, LOG_CONTENT_LEN,
                     "[WARN] Invalid speed at index %d: speed=%.2f", i, speed);
            log_file_write(log_content);
            continue;
        }

        double current_distance = distance(lat, lon, intersection_center_lat, intersection_center_lon);
        log_file_write("Valid vehicle %d: lat=%.6f, lon=%.6f, speed=%.2f, distance=%.2f",
                       i, lat, lon, speed, current_distance);

        if (leading_vehicle.distance == NO_VEHICLE || current_distance < leading_vehicle.distance) {
            leading_vehicle.speed = speed * 0.2778; // km/hr to m/s
            leading_vehicle.distance = current_distance;
        }
    }

    pthread_mutex_lock(&mutex_LV);
    Leading_Vehicles[direct] = leading_vehicle;
    pthread_mutex_unlock(&mutex_LV);

    snprintf(log_content, LOG_CONTENT_LEN,
             "Direction %d updated: Speed=%.2f, Distance=%.2f",
             direct, leading_vehicle.speed, leading_vehicle.distance);
    log_file_write(log_content);

}

int WA_on_registration(void *arg){
    int ret = 0;
    initializationLeadingVehicles(Leading_Vehicles, 4);

    ret = WA_config_init();
    if(ret != 0){
        log_file_write_fatal_error("error WA reading config file: %d", ret);
    }
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),   \
            LOG_CONTENT_LEN - strlen(log_content), \
            "\n WA_on_Registration ");
    log_file_write(log_content);
    log_file_write("frequency: %f", WA_config.warning_freq);
    double freq = WA_config.warning_freq;
    int freq_sec = (int) freq;
    long freq_nsec = (long)((freq - freq_sec) * 1e9);
    create_timer(&WA_Agent_timer_id, NULL, WA_Agent_timer_handler);
    set_timer(WA_Agent_timer_id, freq_sec, freq_nsec, 4, 0);
}


