#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "TIB.h"
#include "TIB_MAP_utils.h"
#include "TIB_SPaT_utils.h"
#include "TIB_config.h"
#include "TIB_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"

void *MAP_packet_tx_loop()
{
    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    int fd = set_timer_fd(TIB_config.MAP_packet_transfer_speed, "MAP_packet_tx_loop");
    int pior_planID = -1;

    if (fd == -1) {
        return NULL;
    }

    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            log_file_write_fatal_error("MAP_packet_tx_loop timer read error");

        int planID = get_plan_id();
        // 切換 plan 的時候才會算一次
        if (planID != pior_planID) {
            if (get_SubPhaseCount() < 0)
                continue;
            if (map_msg_update(map) < 0)
                continue;
            // printf("=========================\n");
            // map_print(map);
            pior_planID = planID;
        }
        // printf("=========================\n");
        // map_print(map);
        OBU_j2735_tx(MapData_Id, map);
    }
    close(fd);
}

void *SPaT_packet_tx_loop()
{
    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    int pior_stepID = -1;
    int pior_second = -1;
    int fd = set_timer_fd(TIB_config.SPaT_packet_transfer_speed, "SPaT_packet_tx_loop");

    if (fd == -1) {
        return NULL;
    }

    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            log_file_write_fatal_error("SPaT_packet_tx_loop timer read error");
        int stepID = get_current_step();
        int second = get_current_second();
        // 在 stepID 換的時候更新
        if (stepID != pior_stepID || pior_second != second) {
            if (spat_msg_update(p_spat) < 0)
                continue;
            pior_stepID = stepID;
            pior_second = second;
        }
        // printf("=========================\n");
        // spat_printf(p_spat);
        OBU_j2735_tx(SPAT_Id, p_spat);
    }
    close(fd);
}

void TIB_send_ack()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TIB_send_ack: malloc");
        perror("TIB_send_ack: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, TIB.id, write_buf.content);
    free(write_buf.content);
    return;
}