#ifndef SPAT_CONFIG_H
#define SPAT_CONFIG_H

#include "typedefine.h"

#define SPAT_CONFIG_FILE FILE_PATH "application/SPaT/config/config.txt"
#define CONFIG_LINE_BUFFER_SIZE 100
#define MAX_CONFIG_VARIABLE_LEN 100

int SPaT_config_init();

/* Return codes of config */
typedef enum SPaT_config_err {
    SPAT_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED = -1,
    CONFIG_INVALID_SPAT_OPEN_FILE = -2
} SPaT_config_err_t;

typedef struct SPaT_config_object {
    uint8_t SPaT_packet_transfer_speed;
    uint8_t signalcount;
    uint8_t intersection_id;
    uint8_t SPaT_dontSend2TC;
} SPaT_config_object_t;

extern SPaT_config_object_t SPaT_config;

#endif