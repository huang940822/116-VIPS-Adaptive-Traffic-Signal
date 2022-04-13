#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "MAP.h"
#include "MAP_packet_tx.h"
#include "MAP_utils.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"

void MAP_packet_tx()
{
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;

    compose_map(&tx_buf, &tx_buf_len);
    if (tx_buf_len <= 0) {
        printf("failed to encode the msg\n");
    } else {
        printf("encode successfully %d\n", tx_buf_len);
    }

    printf("MAP encoded data:\n");
    dump_mem(tx_buf, tx_buf_len);
    OBU_j2735_tx(tx_buf_len, tx_buf);
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