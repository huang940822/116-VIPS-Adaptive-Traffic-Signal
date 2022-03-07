#include <stdio.h>
#include <stdlib.h>

#include "MAP_packet_tx.h"
#include "MAP_utils.h"
#include "com_packet_processing.h"
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