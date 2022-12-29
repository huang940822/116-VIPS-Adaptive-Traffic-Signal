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

void MAP_packet_tx(__sigval_t value)
{
    if (MAP.dontSend2TC)
        return;

    // FILE *fp;
    uint8_t SubPhaseCount = get_SubPhaseCount();
    if(SubPhaseCount > 0){
        if (PhaseOrder != get_PhaseOrder() || prev_phase != get_current_phase()) {
            // update map
            printf("update map information\r\n");
            map_msg_update(map);
            PhaseOrder = get_PhaseOrder();
            prev_phase = get_current_phase();
        }
        OBU_j2735_tx(MapData_Id, map);
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