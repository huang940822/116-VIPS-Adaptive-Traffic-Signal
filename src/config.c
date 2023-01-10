#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "typedefine.h"

config_object_t config = {
    .RSU_name = "S428901   ",
    .RSU_id = 0,
    .RSU_lat = 22.996714,
    .RSU_lon = 120.237009,
    .RSU_elev = 0,
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
};

bool read_uint8_t_from_config_line(char *config_line, uint8_t *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %hhd\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

static bool read_uint32_t_from_config_line(char *config_line, uint32_t *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %u\n", prm_name, val) == 2) {
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

static bool read_double_from_config_line(char *config_line, double *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %lf \n", prm_name, val) == 2) {
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

bool read_string_from_config_line(char *config_line, char *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, MAX_CONFIG_VARIABLE_LEN);
    if (sscanf(config_line, "%s %s\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

static bool read_name_from_config_line(char *config_line, char *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, MAX_CONFIG_VARIABLE_LEN);
    if (sscanf(config_line, "%s \"%[^\"\n]\"\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}
/* 去除前面空白跟後面註解 */
char *trim_space(char *buf)
{
    while (buf != NULL && *buf != '\0') {
        if (*buf != ' ' && *buf != '\t')
            break;
        buf++;
    }
    char *tmp = buf;
    while (tmp != NULL && *tmp != '\0') {
        if (*tmp == '#') {
            *tmp = '\0';
            break;
        }
        tmp++;
    }
    while (buf != tmp) {
        if (*tmp != ' ' && *tmp != '\t' && *tmp != '\0')
            break;
        *tmp = '\0';
        tmp--;
    }
    
    return buf ? ((*buf) ? buf : NULL) : buf;
}

char *read_line(char *read_buf, int read_buf_len, FILE *fp)
{
    while (!feof(fp)) {
        memset(read_buf, 0, read_buf_len);
        fgets(read_buf, read_buf_len, fp);

        char *buf = trim_space(read_buf);
        if (buf == NULL || *buf == '#' || *buf == '\n') {
            continue;
        }
        return buf;
    }
    return NULL;
}

int config_init()
{
    FILE *fp;
    fp = fopen(CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", CONFIG_FILE);
        return CONFIG_INVALID_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully %s", CONFIG_FILE);
    }

    char buf[CONFIG_LINE_BUFFER_SIZE];

    uint8_t uint8_t_val;
    uint32_t uint32_t_val;
    double double_val;
    float float_val;
    float float_val_array[PHASE_COUNT_MAX_NUM];
    char string_val[MAX_CONFIG_VARIABLE_LEN];

    while (!feof(fp)) {
        fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
            continue;
        }

        // RSU name
        if (strstr(buf, "RSU_NAME ")) {
            if (read_name_from_config_line(buf, string_val)) {
                if (strlen(string_val) <= 10) {
                    strncpy(config.RSU_name, string_val, 10);
                    log_file_write("config: RSU_name = %s", config.RSU_name);
                    continue;
                } else {
                    return CONFIG_INVALID_RSU_NAME;
                }
            } else {
                return CONFIG_INVALID_RSU_NAME;
            }
        }
        // RSU id
        if (strstr(buf, "RSU_id ")) {
            if (read_uint32_t_from_config_line(buf, &uint32_t_val)) {
                if (0 <= uint32_t_val && uint32_t_val <= 65535) {
                    config.RSU_id = uint32_t_val;
                    log_file_write("config: RSU_id = %d", config.RSU_id);
                } else {
                    return CONFIG_INVALID_RSU_NAME;
                }
            } else {
                return CONFIG_INVALID_RSU_NAME;
            }
        }
        // RSU id
        if (strstr(buf, "RSU_region ")) {
            if (read_uint32_t_from_config_line(buf, &uint32_t_val)) {
                if (0 <= uint32_t_val && uint32_t_val <= 65535) {
                    config.RSU_region = uint32_t_val;
                    log_file_write("config: RSU_region = %d", config.RSU_region);
                } else {
                    return CONFIG_INVALID_RSU_NAME;
                }
            } else {
                return CONFIG_INVALID_RSU_NAME;
            }
        }
        // rsu lat
        if (strstr(buf, "RSU_LAT ")) {
            if (read_double_from_config_line(buf, &double_val)) {
                if (-90 <= double_val && double_val <= 90) {
                    config.RSU_lat = double_val;
                    log_file_write("config: RSU_lat = %f", config.RSU_lat);
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
            if (read_double_from_config_line(buf, &double_val)) {
                if (-180 < double_val && double_val <= 180) {
                    config.RSU_lon = double_val;
                    log_file_write("config: RSU_lon = %f", config.RSU_lon);
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
                    config.signal_controller_manufacturer = CHENG_LONG;
                    log_file_write("config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);
                    continue;
                } else if (strcmp(string_val, "shan_zhu") == 0) {
                    config.signal_controller_manufacturer = SHAN_ZHU;
                    log_file_write("config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);
                    continue;
                } else if (strcmp(string_val, "shan_zhu_m") == 0) {
                    config.signal_controller_manufacturer = SHAN_ZHU_M;
                    log_file_write("config: signal_controller_manufacturer = %d",
                             config.signal_controller_manufacturer);

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
                    log_file_write("config: signal_status_report_active = %d",
                             config.signal_status_report_active);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_status_report_active = 0;
                    log_file_write("config: signal_status_report_active = %d",
                             config.signal_status_report_active);
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
                    log_file_write("config: signal_adjust_upper_bound_active = %d",
                             config.signal_adjust_upper_bound_active);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_adjust_upper_bound_active = 0;
                    log_file_write("config: signal_adjust_upper_bound_active = %d",
                             config.signal_adjust_upper_bound_active);
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
                    log_file_write("config: signal_adjust_lower_bound_active = %d",
                             config.signal_adjust_lower_bound_active);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.signal_adjust_lower_bound_active = 0;
                    log_file_write("config: signal_adjust_lower_bound_active = %d",
                             config.signal_adjust_lower_bound_active);
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
                    log_file_write("config: signal_adjust_upper_bound_percentage = %f",
                        config.signal_adjust_upper_bound_percentage);
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
                    log_file_write("config: signal_adjust_lower_bound_percentage = %f",
                        config.signal_adjust_lower_bound_percentage);
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
                    log_file_write("config: traffic_compensation_method = %d",
                             config.traffic_compensation_method);
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
                    log_file_write("config: log_middleware_timer_event = %d",
                             config.log_middleware_timer_event);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_middleware_timer_event = 0;
                    log_file_write("config: log_middleware_timer_event = %d",
                             config.log_middleware_timer_event);
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
                    log_file_write("config: log_application_register_event = %d",
                             config.log_application_register_event);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_application_register_event = 0;
                    log_file_write("config: log_application_register_event = %d",
                             config.log_application_register_event);
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
                    log_file_write("config: log_command_buffer = %d",
                             config.log_command_buffer);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_command_buffer = 0;
                    log_file_write("config: log_command_buffer = %d",
                             config.log_command_buffer);
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
                    log_file_write("config: log_signal_packet_rx = %d",
                             config.log_signal_packet_rx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_rx = 0;
                    log_file_write("config: log_signal_packet_rx = %d",
                             config.log_signal_packet_rx);
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
                    log_file_write("config: log_signal_packet_tx = %d",
                             config.log_signal_packet_tx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_tx = 0;
                    log_file_write("config: log_signal_packet_tx = %d",
                             config.log_signal_packet_tx);
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
                    log_file_write("config: log_signal_packet_info = %d",
                             config.log_signal_packet_info);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_signal_packet_info = 0;
                    log_file_write("config: log_signal_packet_info = %d",
                             config.log_signal_packet_info);
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
                    log_file_write("config: log_cloud_packet_rx = %d",
                             config.log_cloud_packet_rx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_cloud_packet_rx = 0;
                    log_file_write( "config: log_cloud_packet_rx = %d",
                             config.log_cloud_packet_rx);
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
                    log_file_write("config: log_cloud_packet_tx = %d",
                             config.log_cloud_packet_tx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_cloud_packet_tx = 0;
                    log_file_write("config: log_cloud_packet_tx = %d",
                             config.log_cloud_packet_tx);
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
                    log_file_write("config: log_OBU_packet_rx = %d",
                             config.log_OBU_packet_rx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_packet_rx = 0;
                    log_file_write("config: log_OBU_packet_rx = %d",
                             config.log_OBU_packet_rx);
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
                    log_file_write("config: log_OBU_packet_tx = %d",
                             config.log_OBU_packet_tx);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_packet_tx = 0;
                    log_file_write("config: log_OBU_packet_tx = %d",
                             config.log_OBU_packet_tx);
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
                    log_file_write("config: .log_OBU_list = %d", config.log_OBU_list);
                    continue;
                } else if (strcmp(string_val, "no") == 0) {
                    config.log_OBU_list = 0;
                    log_file_write("config: .log_OBU_list = %d", config.log_OBU_list);
                    continue;
                } else {
                    return CONFIG_INVALID_LOG_OBU_LIST;
                }
            } else {
                return CONFIG_INVALID_LOG_OBU_LIST;
            }
        }
    }

    fclose(fp);
    return CONFIG_ACCEPT;
}