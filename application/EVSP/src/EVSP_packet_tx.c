#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP.h"
#include "log.h"
// #include "TSP_matrix.h"
#include "EVSP_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "traffic_signal_status_updating.h"

void EVSP_send_ack()
{
    msg_buf_t write_buf;
    write_buf.index = 0;

    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "EVSP_send_ack");

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);
    free(write_buf.content);
    return;
}