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
#include "traffic_signal_status_updating.h"

void MAP_packet_tx(__sigval_t value)
{
    uint8_t SubPhaseCount = get_SubPhaseCount();
    if (SubPhaseCount > 0) {
        map_msg_update(map);
        // map_print(map);
        OBU_j2735_tx(MapData_Id, map);
    }
}

void *SPaT_packet_tx_loop()
{
    // sleep(1);
    int fd = timerfd_create(CLOCK_REALTIME, 0);

    if (fd == -1) {
        log_file_write_fatal_error("SPaT_packet_tx_loop timefd create error.");
    }

    struct itimerspec timerValue;
    memset(&timerValue, 0, sizeof(struct itimerspec));

    int t = 1000000000 / TIB_config.SPaT_packet_transfer_speed;
    timerValue.it_value.tv_sec = t / 1000000000;
    timerValue.it_value.tv_nsec = t % 1000000000;
    timerValue.it_interval.tv_sec = t / 1000000000;
    timerValue.it_interval.tv_nsec = t % 1000000000;

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        log_file_write_fatal_error("SPaT_packet_tx_loop timerfd_settime");
        exit(errno);
    }
    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            log_file_write_fatal_error("SPaT_packet_tx_loop timer read error");
        if (spat_msg_update(p_spat) < 0) {
            continue;
        }
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