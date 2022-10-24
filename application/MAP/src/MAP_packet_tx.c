#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "MAP.h"
#include "MAP_packet_tx.h"
#include "MAP_utils.h"
#include "MAP_config.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

static uint8_t *tx_buf = NULL;
static int tx_buf_len = 0;
static uint8_t PhaseOrder = 255;
static uint8_t prev_phase = 0;
extern MapData *map;

void MAP_packet_tx(__sigval_t value)
{   
    struct timeval start;
    struct timeval end;
    // FILE *fp;
    uint8_t SubPhaseCount = get_SubPhaseCount();
    if(SubPhaseCount > 0){
        // uint8_t current = get_current_phase();
        // uint8_t prev = get_prev_SubPhaseID();
        // uint8_t next = get_next_SubPhaseID();
        // uint8_t SignalCount = get_SignalCount();
        // for(int i = 0;i < SignalCount;i++) {
        //     uint8_t current_status = get_SignalStatus(current-1, i);
        //     uint8_t prev_status = get_SignalStatus(prev-1, i);
        //     uint8_t next_status = get_SignalStatus(next-1, i);
        //     if(current_status & GREEN) {
        //         current_status = LEFT_GREEN | STRAIGHT_GREEN | RIGHT_GREEN;
        //     }
        //     if(prev_status & GREEN) {
        //         prev_status = LEFT_GREEN | STRAIGHT_GREEN | RIGHT_GREEN;
        //     }
        //     if(next_status & GREEN) {
        //         next_status = LEFT_GREEN | STRAIGHT_GREEN | RIGHT_GREEN;
        //     }
        //     if(current_status & prev_status)
        //         update_flag = 1;
        //     if(current_status & next_status)
        //         update_flag = 1;
        // }
        if (PhaseOrder != get_PhaseOrder() || prev_phase != get_current_phase()) {
            // update map
            printf("update map information\r\n");
            gettimeofday(&start,NULL);
            // printf("start:%d\n",start.tv_usec);
            map_msg_update(&map);
            gettimeofday(&end,NULL);
            // printf("update time:%d\n",end.tv_usec-start.tv_usec);
            PhaseOrder = get_PhaseOrder();
            prev_phase = get_current_phase();
        }
        // map_print(map);
        tx_buf_len = compose_map(&tx_buf,map);
        
        if (tx_buf_len <= 0) {
            printf("failed to encode the msg\n");
        } else {
            // printf("encode successfully %d\n", tx_buf_len);
        }
        // fp = fopen("application/MAP/test/rsutest.txt","a+");
        // if(fp!=NULL){
            // char c[14]="\nrevision = ";
            // fwrite(c,sizeof(char),sizeof(c),fp);
            // char x[3];
            // sprintf(x,"%d",MAP_config.Mapconfig -> msgIssueRevision);
            // fwrite(x,sizeof(char),4,fp);
            // if(MAP_config.Mapconfig -> msgIssueRevision==128)
            //     MAP_config.Mapconfig -> msgIssueRevision=0;
        // printf("%d\n",MAP_config.Mapconfig -> msgIssueRevision);
        // MAP_config.Mapconfig -> msgIssueRevision++;
        //     fwrite(tx_buf,sizeof(uint8_t),tx_buf_len,fp);
        // }
        // printf("MAP encoded data:\n");
        // map_dump_mem(tx_buf, tx_buf_len);
        OBU_j2735_tx(tx_buf_len, tx_buf);
    }
}

void MAP_send_ack()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("MAP_send_ack: malloc");
        perror("MAP_send_ack: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, MAP.id, write_buf.content);
    free(write_buf.content);
    return;
}