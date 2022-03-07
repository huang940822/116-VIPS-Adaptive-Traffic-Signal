#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

#include "typedefine.h"

#define MAP_CONFIG_FILE "./application/MAP/config/config.txt"
#define CONFIG_LINE_BUFFER_SIZE 100
#define MAX_CONFIG_VARIABLE_LEN 100

extern MAP_config_object_t MAP_config;

// int MAP_config_init();

/* Return codes of config */ 
typedef enum MAP_config_err {
    MAP_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED = -1,
} MAP_config_err_t;

#endif