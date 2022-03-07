#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>

#include "log.h"
#include "config.h"
#include "j2735_timer_event.h"
#include "SPaT_packet_tx.h"
#include "MAP_packet_tx.h"

void j2735_timer_event_handler(__sigval_t value)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    if(*(uint8_t *)value.sival_ptr == TIMER_EVENT_SPAT_PACKET_TX) {
        if (config.SPaT_packet_tx) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "j2735 timer event: SPaT_packet_tx");
            log_file_write(log_content);
        }

        SPaT_packet_tx();
    }
    else if(*(uint8_t *)value.sival_ptr == TIMER_EVENT_MAP_PACKET_TX) {
        if (config.MAP_packet_tx) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%s", "j2735 timer event: MAP_packet_tx");
            log_file_write(log_content);
        }

        MAP_packet_tx();
    }

}