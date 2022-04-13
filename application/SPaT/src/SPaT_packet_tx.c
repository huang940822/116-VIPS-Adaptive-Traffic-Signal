#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "SPaT.h"
#include "SPaT_packet_tx.h"
#include "SPaT_utils.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"

static uint8_t *tx_buf = NULL;
static int tx_buf_len = 0;
extern SPAT *p_spat;
static int spat_start_delay = 20;
static int spat_update_delay = 0;
// int x = 0;
// FILE *f;
void SPaT_packet_tx()
{
    if(spat_start_delay > 0) { // dalay for the tcbox wrong value
        spat_start_delay--;
        return;
    }
    // if(x==0){
    //     f = fopen("send.txt", "w");
    // }
    int update_result = spat_msg_update(&p_spat);
    if (update_result < 0) {
        return;
    }
    if (!spat_update_delay) {
        tx_buf_len = compose_spat(&tx_buf, p_spat);
        if (tx_buf_len <= 0) {
            printf("failed to encode the msg\n");
        } else {
            printf("encode successfully %d\n", tx_buf_len);
        }
        printf("SPAT encoded data:\n");
        dump_mem(tx_buf, tx_buf_len);
        print_spat(&p_spat);
    }
    spat_update_delay = (spat_update_delay + 1) % 5;
    // char buffer[10];
    // if(x >= 0 && x < 36000) {
    //     unsigned char *p = (unsigned char *)tx_buf;
    //     for (int count = 0; count < tx_buf_len; count++) {
    //         memset(buffer, '\0', 10);
    //         sprintf(buffer,"%02x", p[count]);
    //         fwrite(buffer,1,strlen(buffer), f);
    //     }
    //     memset(buffer, '\0', 10);
    //     sprintf(buffer,"\n");
    //     fwrite(buffer,1,strlen(buffer), f);
    // }
    OBU_j2735_tx(tx_buf_len, tx_buf);
    // if(++x == 36000)
    //     fclose(f);
}

void SPaT_send_ack()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("SPaT_send_ack: malloc");
        perror("SPaT_send_ack: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, SPaT.id, write_buf.content);
    free(write_buf.content);
    return;
}