#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>

#include "APP.h"

app_obj_t APP = {
    .name = "APP",
    .id = 5,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &APP_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &APP_on_registration,
    .next = NULL,
};
int stop = 1; 
int flag = 0;
struct timeval start, end ;
double diff = 0;
int APP_on_camera_packet_rx(void *arg)
{
    int i = 0;
    gettimeofday(&start, NULL);
    while(1){
        gettimeofday(&end, NULL);
        diff = ((double)end.tv_sec + ((double)end.tv_usec)/1000000) - ((double)start.tv_sec + ((double)start.tv_usec)/1000000);
        if (diff >= 0.05){
            printf("diff: %lf\n",diff);
            break;
        }
        else{
            i++;
        }

    }
    //pthread_exit(NULL);
    return 0;
}
int APP_on_registration(void *arg)
{
    ;
}