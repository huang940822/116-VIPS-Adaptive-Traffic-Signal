#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "byte_processing.h"
#include "com_packet_processing.h"
#include "common_packet_tx.h"
#include "config.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

void common_send_ack(uint8_t cmd, uint8_t status)
{
    printf("common send ack, CMD is %d %d\r\n", cmd, status);
    log_file_write("common send ack, CMD is %d %d\r\n", cmd, status);
    msg_buf_t write_buf;
    write_buf.index = 0;
    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "common_send_ack: malloc");

    // cmd
    write_uint8_t(cmd, &write_buf);
    write_uint8_t(status, &write_buf);

    cloud_packet_tx(write_buf.index, COMMON_BROADCAST_ID, write_buf.content);
    free(write_buf.content);
    return;
}

void report_compensation_time(int16_t compensation_time)
{
    printf("report compensation time\r\n");
    msg_buf_t write_buf;
    write_buf.index = 0;

    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "report_compensation_time: malloc");

    // cmd
    write_uint8_t(13, &write_buf);

    // traffic_compensation_method
    write_uint8_t(config.traffic_compensation_method, &write_buf);

    // traffic_compensation_cycle_number
    write_uint8_t(config.traffic_compensation_cycle_number, &write_buf);

    // compensation_time
    write_uint16_t(compensation_time, &write_buf);

    // 因為補償策略二會需要用到 config.phase_weight 所以一併送去雲端
    for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
        write_uint8_t((uint8_t)config.phase_weight[i], &write_buf);
    }
    // 先借用 TSP ID
    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);
    return;
}