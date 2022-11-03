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
#include "dispatcher.h"
#include "log.h"
#include "post_processing.h"
#include "timer_event.h"
#include "typedefine.h"

#define CPS_RX_MODE 1 /* should be 0 ~ 1: 0 -> Applicability-oriented BSM
                                       /* 1 -> Performance-oriented BSM  */

// Below two define CPS_DEBUG_APPLI and CPS_LOG is for CPS testing.
#define CPS_DEBUG_APPLI 16000
#define CPS_LOG 0

# if CPS_LOG
    int32_t id[100000];
    int32_t dscr[100000];
    double lat[100000];
    double lon[100000];  
    double elev[100000];
    float width[100000];  
    float length[100000];  
    int32_t minute[100000] = {0};
    float second[100000];   
    float speed[100000];
    int cnt_log = 0;   
    FILE *fp4;
#endif

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
# if CPS_RX_MODE
        .on_camera_packet_rx = &CPS_on_camera_packet_rx_performance,
# else 
        .on_camera_packet_rx = &CPS_on_camera_packet_rx,
# endif  
        .on_traffic_signal_command_tx = NULL,
        .on_registration = &CPS_on_registration,
        .next = NULL,
    };
# if CPS_DEBUG > 0
#include "server.h"
# if CPS_RX_MODE

    FILE *fp2;
    double tsmps[CPS_DEBUG] = {0.0};
    float obtsmp[CPS_DEBUG] = {0.0};
    void sigintHandlerCPS(int sig_num)
    {
        // signal(SIGINT, sigintHandler);
        for (int i = 0; i < cnt; i++) {
            fprintf(fp2, "%f, %lf, %lf\n", obtsmp[i], tsmp[i], tsmps[i]);
        }
        # if CPS_LOG
            for (int i = 0; i < cnt_log; i++) {
                fprintf(fp4, "%d %d %lf %lf %lf %f %f %f", \
                id[i], dscr[i], lat[i], lon[i], elev[i],\
                width[i], length[i], speed[i]);
                fprintf(fp4, "%d %f \n", minute[i], second[i]);
            }    
            // printf("%d\n", )       
        # endif
        exit(0);
    }
# elif
    FILE *fp2, *fp3;
    int cnter = 0;
    int cnter_2 = 0;
    double tsmps[CPS_DEBUG_APPLI] = {0.0};
    float obtsmp[CPS_DEBUG_APPLI] = {0.0};
    void sigintHandlerCPS(int sig_num)
    {
        for (int i = 0; i < cnter; i++) {
            fprintf(fp2, "%f, %lf\n", obtsmp[i],tsmp[i]);
        }
        for (int i = 0; i < cnter_2; i++) {
            fprintf(fp3, "%lf\n",  tsmps[i]);
        }
        # if CPS_LOG
            for (int i = 0; i < cnt_log; i++) {
                fprintf(fp4, "%d %d %lf %lf %lf %f %f ", \
                id[i], dscr[i], lat[i], lon[i], elev[i],\
                width[i], length[i], speed[i]);
                fprintf(fp4, "%d %f \n", minute[i], second[i]);
            }           
        # endif
        exit(0);
    }    
# endif

# endif

geoinfo_table *g_table = NULL;
buffer_ring_t *DSRC_send_buffer = NULL;
int table_init(geoinfo_table **g_table)
{
    *g_table = calloc(100000, sizeof(geoinfo_table));
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
    int len = 0;
    uint8_t *tx_buf;
    # if CPS_DEBUG > 0 && CPS_DEBUG_APPLI
        obtsmp[cnt] = obstaclelist->tab[1].second;
    # endif
    // printf("\n--------------next msg, count: %d\n", obstaclelist->count);
    for (int i = 0; i < obstaclelist->count; i++) {
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];

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
            &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].length),
            &(obstaclelist->tab[i].width));
        if (bsm_encode(&tx_buf, &len, &(obstaclelist->tab[i]))) {
            // if (buff_ring_push(bsm, DSRC_send_buffer)){
            //     printf("push successed\n");
            //     usleep(50);
            // }
            // encode_cnt++;
            // printf("encode %d times\n", encode_cnt);
            com_send(OBU_com_id, tx_buf, len);
            # if CPS_DEBUG_APPLI > 0 && !CPS_RX_MODE
                double timestamp;
                struct timeval tv;
                gettimeofday(&tv, NULL);
                timestamp = (double)(tv.tv_sec % 60) + tv.tv_usec / 1e6f;
                if (cnter_2 < CPS_DEBUG_APPLI)
                    tsmps[cnter_2] = timestamp;
                cnter_2++;
            # endif
        }
        # if CPS_LOG
            id[cnt_log] = obstaclelist->tab[i].ObstacleID;
            dscr[cnt_log] = obstaclelist->tab[i].description;
            lat[cnt_log] = obstaclelist->tab[i].lat;
            lon[cnt_log] = obstaclelist->tab[i].Long;
            elev[cnt_log] = obstaclelist->tab[i].elev;
            width[cnt_log] = obstaclelist->tab[i].width;
            length[cnt_log] = obstaclelist->tab[i].length;
            minute[cnt_log] = obstaclelist->tab[i].minute;
            second[cnt_log] = obstaclelist->tab[i].second;
            speed[cnt_log] = obstaclelist->tab[i].speed * 0.02;
            cnt_log++;
        # endif
    }
}

int CPS_on_camera_packet_rx_performance(void *arg)
{
    int i = 0;
    ObstacleList *obstaclelist = (ObstacleList *) arg;
    size_t len = 0;
    uint8_t *tx_buf;

    if (obstaclelist->count <= 0)
        return 0;
    # if CPS_DEBUG > 0
        if (cnt < CPS_DEBUG)
            obtsmp[cnt] = obstaclelist->tab[0].second;
    # endif
    for (i = 0; i < obstaclelist->count; i++) {
        geoinfo_table *table = &g_table[obstaclelist->tab[i].ObstacleID];
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
            &(obstaclelist->tab[i].elev), &(obstaclelist->tab[i].length),
            &(obstaclelist->tab[i].width));
        # if CPS_LOG
            id[cnt_log] = obstaclelist->tab[i].ObstacleID;
            dscr[cnt_log] = obstaclelist->tab[i].description;
            lat[cnt_log] = obstaclelist->tab[i].lat;
            lon[cnt_log] = obstaclelist->tab[i].Long;
            elev[cnt_log] = obstaclelist->tab[i].elev;
            width[cnt_log] = obstaclelist->tab[i].width;
            length[cnt_log] = obstaclelist->tab[i].length;
            minute[cnt_log] = obstaclelist->tab[i].minute;
            second[cnt_log] = obstaclelist->tab[i].second;
            speed[cnt_log] = obstaclelist->tab[i].speed * 0.02;
            cnt_log++;
        # endif
    }
    if (bsm_encode_reg(&tx_buf, &len, obstaclelist)) {
        com_send(OBU_com_id, tx_buf, len);
        // buffer_t *bsm = malloc(sizeof(buffer_t));
        // bsm->buff = tx_buf;
        // bsm->size = len;
        // if (buff_ring_push(bsm, DSRC_send_buffer)) {
        //     printf("push successed\n");
        // }
        # if CPS_DEBUG > 0
            double timestamp;
            struct timeval tv;
            gettimeofday(&tv, NULL);
            timestamp = (double)(tv.tv_sec % 60) + tv.tv_usec / 1e6f;
            if (cnt < CPS_DEBUG)
                tsmps[cnt] = timestamp;
            cnt++;
        # endif
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
    // if (DSRC_send_buffer == NULL) {
    //     DSRC_send_buffer = alloc_buffer_ring(24000);
    // }
    # if CPS_DEBUG > 0 
    cnt = 0;
    # if CPS_RX_MODE
        time_t rawtime;
        struct tm *info;
        char buffer[20];
        char name[50];
        memset(buffer, 0, sizeof(buffer));
        time(&rawtime);
        info = localtime(&rawtime);  // 轉換成本地時間表示的分解時間
        strftime(buffer, 20, "%Y-%m-%d_%H:%M:%S", info);
        sprintf(name, "./cps_log/Perf_%s.txt", buffer);
        fp2 = fopen(name, "w");

        signal(SIGINT, sigintHandlerCPS);
    # endif
    # if CPS_DEBUG_APPLI > 0 && !CPS_RX_MODE
        time_t rawtime;
        struct tm *info;
        char buffer[20];
        char name[50];
        memset(buffer, 0, sizeof(buffer));
        time(&rawtime);
        info = localtime(&rawtime);  // 轉換成本地時間表示的分解時間
        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);
        sprintf(name, "./cps_log/Appl_recv_%s.txt", buffer);
        fp2 = fopen(name, "w");
        sprintf(name, "./cps_log/Appl_send_%s.txt", buffer);
        fp3 = fopen(name, "w");
    # endif
    # if CPS_LOG
        time(&rawtime);
        info = localtime(&rawtime);  // 轉換成本地時間表示的分解時間
        strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);
        sprintf(name, "./cps_log/Obstacle_dump_%s.txt", buffer);
        fp4 = fopen(name, "w");
        fprintf(fp4, "ID Type latitude lontitude elevation width length speed minute second\n");
    # endif
    # endif
}