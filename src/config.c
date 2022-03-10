#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "typedefine.h"

config_object_t config = {
    .RSU_id = "S428901   ",
    .RSU_lat = 22.996714,
    .RSU_lon = 120.237009,
    .signal_controller_manufacturer = 1,
    .signal_status_report_active = 0,
    .signal_adjust_upper_bound_active = 1,
    .signal_adjust_lower_bound_active = 1,
    .signal_adjust_upper_bound_percentage = 60,
    .signal_adjust_lower_bound_percentage = 60,
    .traffic_compensation_method = 1,
    .phase_weight = 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    .log_middleware_timer_event = 1,
    .log_application_register_event = 1,
    .log_command_buffer = 1,
    .log_signal_packet_rx = 1,
    .log_signal_packet_tx = 1,
    .log_signal_packet_info = 1,
    .log_cloud_packet_rx = 1,
    .log_cloud_packet_tx = 1,
    .log_OBU_packet_rx = 1,
    .log_OBU_packet_tx = 1,
    .log_OBU_list = 1,
    .SPaT_packet_tx = 1,
    .MAP_packet_tx = 1,
};

static bool read_uint8_t_from_config_line(char *config_line, uint8_t *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %hhd\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}
static bool read_float_from_config_line(char *config_line, float *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %f \n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}
static bool read_float_array_from_config_line(char *config_line, float *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, sizeof(uint8_t) * 8);
    if (sscanf(config_line, "%s %f %f %f %f %f %f %f %f \n", prm_name, &val[0],
               &val[1], &val[2], &val[3], &val[4], &val[5], &val[6],
               &val[7]) == 9) {
        return true;
    } else {
        return false;
    }
}
static bool read_string_from_config_line(char *config_line, char *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, MAX_CONFIG_VARIABLE_LEN);
    if (sscanf(config_line, "%s %s\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

static bool read_id_from_config_line(char *config_line, char *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, MAX_CONFIG_VARIABLE_LEN);
    if (sscanf(config_line, "%s \"%[^\"\n]\"\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

int config_init()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    FILE *fp;
    fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", CONFIG_FILE);
        return CONFIG_INVALID_OPEN_FILE;
    } else {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s opened successfully", CONFIG_FILE);
        log_file_write(log_content);
    }

    char buf[CONFIG_LINE_BUFFER_SIZE];

    uint8_t uint8_t_val;
    float float_val;
    float float_val_array[PHASE_COUNT_MAX_NUM];
    char string_val[MAX_CONFIG_VARIABLE_LEN];

    while (!feof(fp)) {
        memset(log_content, 0, sizeof(log_content));

        fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
            continue;
        }

        // RSU id
        if (strstr(buf, "RSU_ID ")) {
            if (read_id_from_config_line(buf, string_val)) {
                if (strlen(string_val) == 10) {
                    strncpy(config.RSU_id, string_val, 10);
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: RSU_id = %s", config.RSU_id);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_RSU_ID;
                }
            } else {
                return CONFIG_INVALID_RSU_ID;
            }
        }
        // rsu lat
        if (strstr(buf, "RSU_LAT ")) {
            if (read_float_from_config_line(buf, &float_val)) {
                if (float_val >= 0) {
                    config.RSU_lat = float_val;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: RSU_lat = %f", config.RSU_lat);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_RSU_LAT;
                }
            } else {
                return CONFIG_INVALID_RSU_LAT;
            }
        }
        // rsu lon
        if (strstr(buf, "RSU_LON ")) {
            if (read_float_from_config_line(buf, &float_val)) {
                if (float_val >= 0) {
                    config.RSU_lon = float_val;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: RSU_lon = %f", config.RSU_lon);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_RSU_LON;
                }
            } else {
                return CONFIG_INVALID_RSU_LON;
            }
        }
        // signal controller manufacturer
        if (strstr(buf, "SIGNAL_CONTROLLER_MANUFACTURER ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "cheng_long") == 0) {
                    config.signal_controller_manufacturer = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "shan_zhu") == 0) {
                    config.signal_controller_manufacturer = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "shan_zhu_m") == 0) {
                    config.signal_controller_manufacturer = 2;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);
                    log_file_write(log_content);

                } else {
                    return CONFIG_INVALID_SIGNAL_CONTROLLER_MANUFACTURER;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_CONTROLLER_MANUFACTURER;
            }
        }
        // signal status report active
        if (strstr(buf, "SIGNAL_STATUS_REPORT_ACTIVE ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.signal_status_report_active = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_status_report_active = %d",
                             config.signal_status_report_active);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_status_report_active = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_status_report_active = %d",
                             config.signal_status_report_active);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SIGNAL_STATUS_REPORT_ACTIVE;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_STATUS_REPORT_ACTIVE;
            }
        }
        // signal adjust upper bound active
        if (strstr(buf, "SIGNAL_ADJUST_UPPER_BOUND_ACTVE ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.signal_adjust_upper_bound_active = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_adjust_upper_bound_active = %d",
                             config.signal_adjust_upper_bound_active);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_adjust_upper_bound_active = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_adjust_upper_bound_active = %d",
                             config.signal_adjust_upper_bound_active);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_ACTVE;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_ACTVE;
            }
        }
        // signal adjust lower bound active
        if (strstr(buf, "SIGNAL_ADJUST_LOWER_BOUND_ACTVE ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.signal_adjust_lower_bound_active = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_adjust_lower_bound_active = %d",
                             config.signal_adjust_lower_bound_active);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_adjust_lower_bound_active = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: signal_adjust_lower_bound_active = %d",
                             config.signal_adjust_lower_bound_active);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_ACTVE;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_ACTVE;
            }
        }
        // signal adjust upper bound percentage
        if (strstr(buf, "SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE ")) {
            if (read_float_from_config_line(buf, &float_val)) {
                if (float_val >= 0) {
                    config.signal_adjust_upper_bound_percentage = float_val;
                    snprintf(
                        log_content + strlen(log_content),
                        LOG_CONTENT_LEN - strlen(log_content),
                        "config: signal_adjust_upper_bound_percentage = %f",
                        config.signal_adjust_upper_bound_percentage);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE;
            }
        }
        // signal adjust lower bound percentage
        if (strstr(buf, "SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE ")) {
            if (read_float_from_config_line(buf, &float_val)) {
                if (float_val >= 0) {
                    config.signal_adjust_lower_bound_percentage = float_val;
                    snprintf(
                        log_content + strlen(log_content),
                        LOG_CONTENT_LEN - strlen(log_content),
                        "config: signal_adjust_lower_bound_percentage = %f",
                        config.signal_adjust_lower_bound_percentage);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE;
                }
            } else {
                return CONFIG_INVALID_SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE;
            }
        }
        // traffic compensation method
        if (strstr(buf, "TRAFFIC_COMPENSATION_METHOD ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    config.traffic_compensation_method = uint8_t_val;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: traffic_compensation_method = %d",
                             config.traffic_compensation_method);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_TRAFFIC_COMPENSATION_METHOD;
                }
            } else {
                return CONFIG_INVALID_TRAFFIC_COMPENSATION_METHOD;
            }
        }
        // phase weight
        if (strstr(buf, "PHASE_WEIGHT ")) {
            if (read_float_array_from_config_line(buf, float_val_array)) {
                for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
                    if (float_val_array[i] >= 0) {
                        config.phase_weight[i] = float_val_array[i];
                    } else {
                        return CONFIG_INVALID_PHASE_WEIGHT;
                    }
                }
                continue;
            } else {
                return CONFIG_INVALID_PHASE_WEIGHT;
            }
        }
        // log middleware timer event
        if (strstr(buf, "LOG_MIDDLEWARE_TIMER_EVENT ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_middleware_timer_event = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_middleware_timer_event = %d",
                             config.log_middleware_timer_event);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_middleware_timer_event = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_middleware_timer_event = %d",
                             config.log_middleware_timer_event);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_MIDDLEWARE_TIMER_EVENT;
                }
            } else {
                return CONFIG_INVALID_LOG_MIDDLEWARE_TIMER_EVENT;
            }
        }
        // log applicatoin register event
        if (strstr(buf, "LOG_APPLICATION_REGISTER_EVENT ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_application_register_event = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_application_register_event = %d",
                             config.log_application_register_event);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_application_register_event = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_application_register_event = %d",
                             config.log_application_register_event);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_APPLICATION_REGISTER_EVENT;
                }
            } else {
                return CONFIG_INVALID_LOG_APPLICATION_REGISTER_EVENT;
            }
        }
        // log command buffer
        if (strstr(buf, "LOG_COMMAND_BUFFER ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_command_buffer = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_command_buffer = %d",
                             config.log_command_buffer);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_command_buffer = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_command_buffer = %d",
                             config.log_command_buffer);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_COMMAND_BUFFER;
                }
            } else {
                return CONFIG_INVALID_LOG_COMMAND_BUFFER;
            }
        }
        // log signal packet rx
        if (strstr(buf, "LOG_SIGNAL_PACKET_RX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_signal_packet_rx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_rx = %d",
                             config.log_signal_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_rx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_rx = %d",
                             config.log_signal_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_SIGNAL_PACKET_RX;
                }
            } else {
                return CONFIG_INVALID_LOG_SIGNAL_PACKET_RX;
            }
        }
        // log signal packet tx
        if (strstr(buf, "LOG_SIGNAL_PACKET_TX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_signal_packet_tx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_tx = %d",
                             config.log_signal_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_tx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_tx = %d",
                             config.log_signal_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_SIGNAL_PACKET_TX;
                }
            } else {
                return CONFIG_INVALID_LOG_SIGNAL_PACKET_TX;
            }
        }
        // log signal packet info
        if (strstr(buf, "LOG_SIGNAL_PACKET_INFO ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_signal_packet_info = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_info = %d",
                             config.log_signal_packet_info);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_info = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_signal_packet_info = %d",
                             config.log_signal_packet_info);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_SIGNAL_PACKET_INFO;
                }
            } else {
                return CONFIG_INVALID_LOG_SIGNAL_PACKET_INFO;
            }
        }
        // log cloud packet rx
        if (strstr(buf, "LOG_CLOUD_PACKET_RX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_cloud_packet_rx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_cloud_packet_rx = %d",
                             config.log_cloud_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_cloud_packet_rx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_cloud_packet_rx = %d",
                             config.log_cloud_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_CLOUD_PACKET_RX;
                }
            } else {
                return CONFIG_INVALID_LOG_CLOUD_PACKET_RX;
            }
        }
        // log cloud packet tx
        if (strstr(buf, "LOG_CLOUD_PACKET_TX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_cloud_packet_tx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_cloud_packet_tx = %d",
                             config.log_cloud_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_cloud_packet_tx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_cloud_packet_tx = %d",
                             config.log_cloud_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_CLOUD_PACKET_TX;
                }
            } else {
                return CONFIG_INVALID_LOG_CLOUD_PACKET_TX;
            }
        }
        // log OBU packet rx
        if (strstr(buf, "LOG_OBU_PACKET_RX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_OBU_packet_rx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_OBU_packet_rx = %d",
                             config.log_OBU_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_packet_rx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_OBU_packet_rx = %d",
                             config.log_OBU_packet_rx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_OBU_PACKET_RX;
                }
            } else {
                return CONFIG_INVALID_LOG_OBU_PACKET_RX;
            }
        }
        // log OBU packet tx
        if (strstr(buf, "LOG_OBU_PACKET_TX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_OBU_packet_tx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_OBU_packet_tx = %d",
                             config.log_OBU_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_packet_tx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: log_OBU_packet_tx = %d",
                             config.log_OBU_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_OBU_PACKET_TX;
                }
            } else {
                return CONFIG_INVALID_LOG_OBU_PACKET_TX;
            }
        }
        // log OBU list
        if (strstr(buf, "LOG_OBU_LIST ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.log_OBU_list = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .log_OBU_list = %d", config.log_OBU_list);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_list = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .log_OBU_list = %d", config.log_OBU_list);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_OBU_LIST;
                }
            } else {
                return CONFIG_INVALID_LOG_OBU_LIST;
            }
        }
        // SPaT packet tx
        if (strstr(buf, "SPAT_PACKET_TX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.SPaT_packet_tx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .SPaT_packet_tx = %d",
                             config.SPaT_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.SPaT_packet_tx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .SPaT_packet_tx = %d",
                             config.SPaT_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_SPAT_PACKET_TX;
                }
            } else {
                return CONFIG_INVALID_SPAT_PACKET_TX;
            }
        }
        // MAP packet tx
        if (strstr(buf, "MAP_PACKET_TX ")) {
            if (read_string_from_config_line(buf, string_val)) {
                if (strcmp(string_val, "yes") == 0) {
                    config.MAP_packet_tx = 1;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .MAP_packet_tx = %d",
                             config.SPaT_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.MAP_packet_tx = 0;
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "config: .MAP_packet_tx = %d",
                             config.SPaT_packet_tx);
                    log_file_write(log_content);
                    continue;
                } else {
                    return CONFIG_INVALID_MAP_PACKET_TX;
                }
            } else {
                return CONFIG_INVALID_MAP_PACKET_TX;
            }
        }
    }

    fclose(fp);
    return CONFIG_ACCEPT;
}