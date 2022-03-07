#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include "CPS.h"
#include "log.h"
#include "config.h"
#include "timer_event.h"
#include "byte_processing.h"
#include "BSM.h"
#include "com_packet_processing.h"
#include "ObstacleList.h"
#include "post_processing.h"
#include "uthash.h"

app_obj_t CPS_E = {
    .name = "CPS_E",
    .id = 1,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &CPS_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &CPS_on_registration,
    .next = NULL,
};
app_obj_t CPS_W = {
    .name = "CPS_W",
    .id = 2,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &CPS_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &CPS_on_registration,
    .next = NULL,
};
app_obj_t CPS_S = {
    .name = "CPS_S",
    .id = 3,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &CPS_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &CPS_on_registration,
    .next = NULL,
};
app_obj_t CPS_N = {
    .name = "CPS_N",
    .id = 4,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &CPS_on_camera_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &CPS_on_registration,
    .next = NULL,
};
int mean = 0;
int cnt_e = 0;
int cnt_w = 0;
int cnt_s = 0;
int cnt_n = 0;
double timeline_e[1000];
double timeline_w[1000];
double timeline_s[1000];
double timeline_n[1000];
int sum = 0;
int Mean[100] = {0};
int encode_cnt = 0;
struct timeval start, end, diff;
geoinfo_table *g_table = NULL;
int table_init(geoinfo_table **g_table){
    *g_table = calloc(10000, sizeof(geoinfo_table));
    if (g_table == NULL){
        return -1;
    }
    int i = 0;
    for(i = 0;i<10000;i++){
       (*g_table)[i].lat = -1.0;
       (*g_table)[i].lon = -1.0;
       (*g_table)[i].second = -1; 
    }
}
int CPS_on_camera_packet_rx(void *arg)
{
    ObstacleList *obstaclelist;
    obstaclelist = (ObstacleList *)arg;
    size_t len = 0;
    int8_t *tx_buf;
    // printf("\n--------------next msg, count: %d\n", obstaclelist->count);
    for(int i = 0; i<obstaclelist->count; i++){
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];
        // printf("\n");

        if(table->second == -1.0 || (obstaclelist->tab[i].second - table->second) > 10.0){//empty or expired
            // table->lat = obstaclelist->tab[i].lat;
            // table->lon = obstaclelist->tab[i].Long;
            // table->second = obstaclelist->tab[i].second;
        }
        else{
            obstaclelist->tab[i].speed = (int)((distance(table->lat, table->lon, obstaclelist->tab[i].lat, obstaclelist->tab[i].Long)/((obstaclelist->tab[i].second - table->second)))/(0.02));
            obstaclelist->tab[i].heading = (int)(bearing(table->lat, table->lon, obstaclelist->tab[i].lat, obstaclelist->tab[i].Long) / 0.0125);
        }
        table->lat = obstaclelist->tab[i].lat;
        table->lon = obstaclelist->tab[i].Long;
        table->second = obstaclelist->tab[i].second;

        printf("obstaclelist->tab[i].speed : %d \n", obstaclelist->tab[i].speed);
        // printf("obstaclelist->tab[i].heading : %d \n", obstaclelist->tab[i].heading);

        transfer_datatype(&(obstaclelist->tab[i].lat), &(obstaclelist->tab[i].Long), &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].hight), &(obstaclelist->tab[i].width));
        if (bsm_encode(&tx_buf,&len, &(obstaclelist->tab[i]))){
            encode_cnt++;
            // printf("encode %d times\n", encode_cnt);
            OBU_j2735_tx(len,tx_buf);
            //for(int i = 0;i < 1000;i++);
            //usleep(50);
        }
    }
}
int CPS_on_camera_packet_rx_performance(void *arg)
{
    int i = 0;
    ObstacleList *obstaclelist = (ObstacleList *)arg;  
    size_t len = 0;
    int8_t *tx_buf;
    for(i = 0;i<obstaclelist->count;i++){
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];
        // printf("\n");

        if(table->second == -1.0 || (obstaclelist->tab[i].second - table->second) > 10.0){//empty or expired
            // table->lat = obstaclelist->tab[i].lat;
            // table->lon = obstaclelist->tab[i].Long;
            // table->second = obstaclelist->tab[i].second;
        }
        else{
            obstaclelist->tab[i].speed = (int)((distance(table->lat, table->lon, obstaclelist->tab[i].lat, obstaclelist->tab[i].Long)/((obstaclelist->tab[i].second - table->second)))/(0.02));
            obstaclelist->tab[i].heading = (int)(bearing(table->lat, table->lon, obstaclelist->tab[i].lat, obstaclelist->tab[i].Long) / 0.0125);
        }
        table->lat = obstaclelist->tab[i].lat;
        table->lon = obstaclelist->tab[i].Long;
        table->second = obstaclelist->tab[i].second;

        transfer_datatype(&(obstaclelist->tab[i].lat), &(obstaclelist->tab[i].Long), &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].hight), &(obstaclelist->tab[i].width));
    }
    if(bsm_encode_reg(&tx_buf,&len, obstaclelist)){
        OBU_BSM_tx(len,tx_buf);
        gettimeofday(&end, NULL);
        if (obstaclelist->dirct == 0){
            // FILE * fptr;
            // timeline_e[cnt_e] = (double)end.tv_sec + ((double)end.tv_usec)/1000000;
            // cnt_e++;
            // if (cnt_e == 1000){
            //     fptr = fopen( "../../Latency_Test/end_time_e.txt","w+" );
            //     for (i = 0;i<1000;i++){
            //         fprintf(fptr,"%lf\n",timeline_e[i]); 
            //     }
            //     cnt_e = 0;
            //     fclose(fptr);
            // }
            
        }
        else if (obstaclelist->dirct == 1){
            // FILE * fptr;
            // timeline_w[cnt_w] = (double)end.tv_sec + ((double)end.tv_usec)/1000000;
            // cnt_w++;
            // printf("cnt_w: %d\n", cnt_w);
            // if (cnt_w == 1000){
            //     fptr = fopen( "../../Latency_Test/end_time_w.txt","w+" );
            //     for (i = 0;i<1000;i++){
            //         fprintf(fptr,"%lf\n",timeline_w[i]); 
            //     }
            //     cnt_w = 0;
            //     fclose(fptr);
            // }
        }
        else if (obstaclelist->dirct == 2){
            // FILE * fptr;
            // timeline_s[cnt_s] = (double)end.tv_sec + ((double)end.tv_usec)/1000000;
            // cnt_s++;
            // if (cnt_s == 1000){
            //     fptr = fopen( "../../Latency_Test/end_time_s.txt","w+" );
            //     for (i = 0;i<1000;i++){
            //         fprintf(fptr,"%lf\n",timeline_s[i]); 
            //     }
            //     cnt_s = 0;
            //     fclose(fptr);
            // }
        }
        else if (obstaclelist->dirct == 3){
            // FILE * fptr;
            // timeline_n[cnt_n] = (double)end.tv_sec + ((double)end.tv_usec)/1000000;
            // cnt_n++;
            // if (cnt_n == 1000){
            //     fptr = fopen( "../../Latency_Test/end_time_n.txt","w+" );
            //     for (i = 0;i<1000;i++){
            //         fprintf(fptr,"%lf\n",timeline_n[i]); 
            //     }
            //     cnt_n = 0;
            //     fclose(fptr);
            // }
        }   
    }
}
int CPS_on_registration(void *arg)
{
    table_init(&g_table);
}