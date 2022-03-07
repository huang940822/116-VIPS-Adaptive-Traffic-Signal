#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "BSM_codec.h"
#include "CPS.h"
#include "ObstacleList.h"
#include "buffer.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "log.h"
#include "post_processing.h"
#include "timer_event.h"

app_obj_t CPS = {
    .name = "CPS",
    .id = 3,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = &CPS_on_camera_packet_rx_performance,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &CPS_on_registration,
    .next = NULL,
};
// int mean = 0;
// int cnt_e = 0;
// int cnt_w = 0;
// int cnt_s = 0;
// int cnt_n = 0;
// double timeline_e[1000];
// double timeline_w[1000];
// double timeline_s[1000];
// double timeline_n[1000];
// int sum = 0;
// int Mean[100] = {0};
int encode_cnt = 0;
// struct timeval start, end, diff;
geoinfo_table *g_table = NULL;
buffer_ring_t *DSRC_send_buffer = NULL;
int table_init(geoinfo_table **g_table)
{
    *g_table = calloc(1000, sizeof(geoinfo_table));
    if (g_table == NULL) {
        return -1;
    }
    int i = 0;
    for (i = 0; i < 1000; i++) {
        (*g_table)[i].lat = -1.0;
        (*g_table)[i].lon = -1.0;
        (*g_table)[i].second = -1;
    }
}
int CPS_on_camera_packet_rx(void *arg)
{
    ObstacleList *obstaclelist;
    obstaclelist = (ObstacleList *) arg;
    size_t len = 0;
    int8_t *tx_buf;
    // printf("\n--------------next msg, count: %d\n", obstaclelist->count);
    for (int i = 0; i < obstaclelist->count; i++) {
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];
        // printf("\n");

        if (table->second != -1 &&
            (obstaclelist->tab[i].second - table->second) <
                10.0) {  // not empty or expired
            obstaclelist->tab[i].speed =
                (int) ((distance(table->lat, table->lon,
                                 obstaclelist->tab[i].lat,
                                 obstaclelist->tab[i].Long) /
                        ((obstaclelist->tab[i].second - table->second))) /
                       (0.02));
            obstaclelist->tab[i].heading =
                (int) (bearing(table->lat, table->lon, obstaclelist->tab[i].lat,
                               obstaclelist->tab[i].Long) /
                       0.0125);
        }
        table->lat = obstaclelist->tab[i].lat;
        table->lon = obstaclelist->tab[i].Long;
        table->second = obstaclelist->tab[i].second;

        transfer_datatype(
            &(obstaclelist->tab[i].lat), &(obstaclelist->tab[i].Long),
            &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].hight),
            &(obstaclelist->tab[i].width));
        if (bsm_encode(&tx_buf, &len, &(obstaclelist->tab[i]))) {
            buffer_t *bsm = malloc(sizeof(buffer_t));
            bsm->buff = tx_buf;
            bsm->size = len;
            // if (buff_ring_push(bsm, DSRC_send_buffer)){
            //     printf("push successed\n");
            //     usleep(50);
            // }
            // encode_cnt++;
            // printf("encode %d times\n", encode_cnt);
            OBU_BSM_tx(len, tx_buf);
            // //for(int i = 0;i < 1000;i++);
            // usleep(50);
        }
    }
}
#define END_TIME_E FILE_PATH "Latency_Test/end_time_e.txt"
#define END_TIME_W FILE_PATH "Latency_Test/end_time_w.txt"
#define END_TIME_S FILE_PATH "Latency_Test/end_time_s.txt"
#define END_TIME_N FILE_PATH "Latency_Test/end_time_n.txt"

int CPS_on_camera_packet_rx_performance(void *arg)
{
    int i = 0;
    ObstacleList *obstaclelist = (ObstacleList *) arg;
    size_t len = 0;
    int8_t *tx_buf;
    for (i = 0; i < obstaclelist->count; i++) {
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];
        if (table->second == -1.0 ||
            (obstaclelist->tab[i].second - table->second) >
                10.0) {  // empty or expired
            // table->lat = obstaclelist->tab[i].lat;
            // table->lon = obstaclelist->tab[i].Long;
            // table->second = obstaclelist->tab[i].second;
        } else {
            obstaclelist->tab[i].speed =
                (int) ((distance(table->lat, table->lon,
                                 obstaclelist->tab[i].lat,
                                 obstaclelist->tab[i].Long) /
                        ((obstaclelist->tab[i].second - table->second))) /
                       (0.02));

            obstaclelist->tab[i].heading =
                (int) (bearing(table->lat, table->lon, obstaclelist->tab[i].lat,
                               obstaclelist->tab[i].Long) /
                       0.0125);
        }
        table->lat = obstaclelist->tab[i].lat;
        table->lon = obstaclelist->tab[i].Long;
        table->second = obstaclelist->tab[i].second;

        transfer_datatype(
            &(obstaclelist->tab[i].lat), &(obstaclelist->tab[i].Long),
            &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].hight),
            &(obstaclelist->tab[i].width));
    }
    if (bsm_encode_reg(&tx_buf, &len, obstaclelist)) {
        buffer_t *bsm = malloc(sizeof(buffer_t));
        bsm->buff = tx_buf;
        bsm->size = len;
        if (buff_ring_push(bsm, DSRC_send_buffer)) {
            printf("push successed\n");
        }
        // OBU_BSM_tx(len,tx_buf);
        // gettimeofday(&end, NULL);
        /* if (obstaclelist->dirct == 0){
             FILE * fptr;
             timeline_e[cnt_e] = (double)end.tv_sec +
         ((double)end.tv_usec)/1000000; cnt_e++;

             /*if (cnt_e == 1000){
                 fptr = fopen( "../../Latency_Test/end_time_e.txt","w+" );

                 for (i = 0;i<1000;i++){
                     fprintf(fptr,"%lf\n",timeline_e[i]);
                 }
                 cnt_e = 0;
                 fclose(fptr);
             }

         }
         else if (obstaclelist->dirct == 1){
             FILE * fptr;
             timeline_w[cnt_w] = (double)end.tv_sec +
         ((double)end.tv_usec)/1000000; cnt_w++; printf("cnt_w: %d\n", cnt_w);
             if (cnt_w == 1000){
                 fptr = fopen( END_TIME_W,"w+" );
                 for (i = 0;i<1000;i++){
                     fprintf(fptr,"%lf\n",timeline_w[i]);
                 }
                 cnt_w = 0;
                 fclose(fptr);
             }
         }
         else if (obstaclelist->dirct == 2){
             FILE * fptr;
             timeline_s[cnt_s] = (double)end.tv_sec +
         ((double)end.tv_usec)/1000000; cnt_s++; if (cnt_s == 1000){ fptr =
         fopen( END_TIME_S,"w+" ); for (i = 0;i<1000;i++){
                     fprintf(fptr,"%lf\n",timeline_s[i]);
                 }
                 cnt_s = 0;
                 fclose(fptr);
             }
         }
         else if (obstaclelist->dirct == 3){
             FILE * fptr;
             timeline_n[cnt_n] = (double)end.tv_sec +
         ((double)end.tv_usec)/1000000; cnt_n++; if (cnt_n == 1000){ fptr =
         fopen( END_TIME_N,"w+" ); for (i = 0;i<1000;i++){
                     fprintf(fptr,"%lf\n",timeline_n[i]);
                 }
                 cnt_n = 0;
                 fclose(fptr);
             }
         } */
    }
    if (obstaclelist->tab != NULL) {
        free(obstaclelist->tab);
    }
    if (obstaclelist != NULL) {
        free(obstaclelist);
    }
}
int CPS_on_registration(void *arg)
{
    // if(g_table){
    //     free(g_table);
    // }
    if (g_table == NULL)
        table_init(&g_table);
    if (DSRC_send_buffer == NULL) {
        DSRC_send_buffer = alloc_buffer_ring(24000);
    }
}