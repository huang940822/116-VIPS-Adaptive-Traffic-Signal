#ifndef SPM_CONFIG_H
#define SPM_CONFIG_H

#define SPM_CONFIG_FILE FILE_PATH "application/SPM/config/config.txt"

#include "typedefine.h"

int SPM_config_init();

/* Return codes of config */
typedef enum SPM_config_err {
    SPM_CONFIG_ACCEPT = 0,
    CONFIG_INVALID_SPM_PACKET = -1,
    CONFIG_INVALID_SPM_OPEN_FILE = -2
} SPM_config_err_t;

typedef struct SPM_config_object {
    uint8_t spm_host_obu_packet_timeout;
    uint8_t SPM_packet_transfer_speed;
    uint8_t SPM_dontSend2TC;
} SPM_config_object_t;

extern SPM_config_object_t SPM_config;

#endif