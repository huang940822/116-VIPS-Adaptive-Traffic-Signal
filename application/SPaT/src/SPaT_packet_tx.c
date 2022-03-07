#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "com_packet_processing.h"
#include "SPaT_packet_tx.h"
#include "SPaT_utils.h"

static uint8_t *tx_buf = NULL;
static int tx_buf_len = 0;
extern SPAT *p_spat;
static int spat_update_delay = 4;
void SPaT_packet_tx()
{
    int update_result= spat_msg_update(&p_spat);
    if(spat_msg_update(&p_spat) < 0) {
        return;
    }
    spat_update_delay = (spat_update_delay + 1) % 5;
    if(!spat_update_delay)
        tx_buf_len = compose_spat(&tx_buf, p_spat);
    if (tx_buf_len <= 0)
    {
        printf("failed to encode the msg\n");
    }
    else
    {
        printf("encode successfully %d\n", tx_buf_len);
    }
    printf("SPAT encoded data:\n");
    dump_mem(tx_buf, tx_buf_len);
    OBU_j2735_tx(tx_buf_len,tx_buf);
}