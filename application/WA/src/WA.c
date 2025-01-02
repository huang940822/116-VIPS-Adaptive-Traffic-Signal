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

#define NO_VEHICLE -1.0 

CCI Leading_Vehicles[4]; // Leading Vehicles for four directions NSWE
timer_t WA_Agent_timer_id;
pthread_mutex_t mutex_state = PTHREAD_MUTEX_INITIALIZER;

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
    //[TODO] .on_radar_packet_rx = &WA_on_detected_object_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &WA_on_registration,
    .next = NULL,
};

int WA_on_camera_packet_rx(void *arg){ /* [TODO] 改名稱 */
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    // Add a log message indicating the function was called
    snprintf(log_content + strlen(log_content), 
             LOG_CONTENT_LEN - strlen(log_content), 
             "\n WA_on_camera_packet_rx called.");
    log_file_write(log_content);
    ObstacleList *obstaclelist = (ObstacleList *) arg;
    int direct = obstaclelist->dirct;
    double traffic_light_lat = WA_config.traffic_light.tab[direct].Traffic_light_lat;
    double traffic_light_lon = WA_config.traffic_light.tab[direct].Traffic_light_lon;
    CCI leading_vehicle;
    if(obstaclelist->count != 0){
        leading_vehicle.speed = obstaclelist->tab[0].speed;
        leading_vehicle.distance = distance(obstaclelist->tab[0].lat, obstaclelist->tab[0].Long, traffic_light_lat, traffic_light_lon);
        log_file_write("lat:%lf ,lon:%lf", obstaclelist->tab[0].lat, obstaclelist->tab[0].Long);
        for(int i = 1; i < obstaclelist->count; i++) {
            double current_vehicle_distance = distance(obstaclelist->tab[i].lat, obstaclelist->tab[i].Long, traffic_light_lat, traffic_light_lon);
            if(current_vehicle_distance < leading_vehicle.distance){
                leading_vehicle.speed = obstaclelist->tab[i].speed;
                leading_vehicle.distance = current_vehicle_distance;
            }
        }
    }
    pthread_mutex_lock(&mutex_state);
    Leading_Vehicles[direct] = leading_vehicle;
    pthread_mutex_unlock(&mutex_state);

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
}


