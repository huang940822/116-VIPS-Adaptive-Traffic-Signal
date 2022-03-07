#ifndef SPAT_CONFIG_H
#define SPAT_CONFIG_H

#include "typedefine.h"

#define SPAT_CONFIG_FILE "./application/SPaT/config/config.txt"
#define CONFIG_LINE_BUFFER_SIZE 100
#define MAX_CONFIG_VARIABLE_LEN 100
extern SPaT_config_object_t SPaT_config;

int SPaT_config_init();

/* Return codes of config */
typedef enum SPaT_config_err {
    SPAT_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED = -1,
} SPaT_config_err_t;


#endif