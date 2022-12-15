#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include "typedefine.h"

#define CONFIG_FILE FILE_PATH "config/config.txt"
#define CONFIG_LINE_BUFFER_SIZE 100
#define MAX_CONFIG_VARIABLE_LEN 100

int config_init();

typedef struct config_object {
    char RSU_name[RSU_NAME_MAX_LEN];
    uint32_t RSU_id;
    uint32_t RSU_region;
    double RSU_lat;
    double RSU_lon;
    double RSU_elev;
    uint8_t signal_controller_manufacturer;

    bool signal_status_report_active;
    bool signal_adjust_upper_bound_active;
    bool signal_adjust_lower_bound_active;
    float signal_adjust_upper_bound_percentage;
    float signal_adjust_lower_bound_percentage;
    uint8_t traffic_compensation_method;
    float phase_weight[PHASE_COUNT_MAX_NUM];

    bool log_middleware_timer_event;
    bool log_application_register_event;
    bool log_command_buffer;  // what for??
    bool log_signal_packet_rx;
    bool log_signal_packet_tx;
    bool log_signal_packet_info;
    bool log_cloud_packet_rx;
    bool log_cloud_packet_tx;
    bool log_OBU_packet_rx;
    bool log_OBU_packet_tx;
    bool log_OBU_list;
} config_object_t;

extern config_object_t config;

/* Return codes of config */
typedef enum config_err {
    CONFIG_ACCEPT = 0,
    CONFIG_INVALID_RSU_NAME = -1,
    CONFIG_INVALID_RSU_ID = -2,
    CONFIG_INVALID_RSU_LAT = -3,
    CONFIG_INVALID_RSU_LON = -4,
    CONFIG_INVALID_SIGNAL_CONTROLLER_MANUFACTURER = -5,
    CONFIG_INVALID_SIGNAL_STATUS_REPORT_ACTIVE = -6,
    CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_ACTVE = -7,
    CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_ACTVE = -8,
    CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE = -9,
    CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE = -10,
    CONFIG_INVALID_LOG_MIDDLEWARE_TIMER_EVENT = -11,
    CONFIG_INVALID_LOG_APPLICATION_REGISTER_EVENT = -12,
    CONFIG_INVALID_LOG_COMMAND_BUFFER = -13,
    CONFIG_INVALID_LOG_SIGNAL_PACKET_RX = -14,
    CONFIG_INVALID_LOG_SIGNAL_PACKET_TX = -15,
    CONFIG_INVALID_LOG_SIGNAL_PACKET_INFO = -16,
    CONFIG_INVALID_LOG_CLOUD_PACKET_RX = -17,
    CONFIG_INVALID_LOG_CLOUD_PACKET_TX = -18,
    CONFIG_INVALID_LOG_OBU_PACKET_RX = -19,
    CONFIG_INVALID_LOG_OBU_PACKET_TX = -20,
    CONFIG_INVALID_LOG_OBU_LIST = -21,
    CONFIG_INVALID_TRAFFIC_COMPENSATION_METHOD = -22,
    CONFIG_INVALID_PHASE_WEIGHT = -23,
    CONFIG_INVALID_OPEN_FILE = -99,
} config_err_t;


char *trim_space(char *buf);

char *read_line(char *read_buf, int read_buf_len, FILE *fp);

bool read_uint8_t_from_config_line(char *config_line, uint8_t *val);
#endif