#include "TIB_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

LOG_USE_MODULE(TIB);

TIB_config_object_t TIB_config = {
    .MAP_packet_transfer_speed = 1,
    .SPaT_packet_transfer_speed = 10,
    .general_packet_time_per_cycle = 0.5,
    .MAP_packet_cycle_per_transfer = 1,
    .SPaT_packet_cycle_per_transfer = 2,
    .TIM_packet_cycle_per_transfer = 2,
    .EVA_packet_cycle_per_transfer = 2,
    .RSA_packet_cycle_per_transfer = 2,  
    .PSM_packet_cycle_per_transfer = 2,  
    .TIB_dontSend2TC = 1,
    .lane_list = {0},
    .connectsTo_list = {0},
    .MAP_lane_approach = {0},
    .TIM_table = {0},
    
};

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

static bool read_int_from_config_line(char *config_line, int *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %d \n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}
int TIB_config_init()
{
    FILE *fp;
    /* Get file path */
    char file_path[256] = {0};
    char rsu_name[RSU_NAME_MAX_LEN + 1] = {0};
    strncpy(rsu_name, config.RSU_name, RSU_NAME_MAX_LEN);
    trim_space(rsu_name);

    if (rsu_name == NULL) {
        LOG_MSG_FATAL("error name %s", config.RSU_name);
    }

    memset(file_path, 0, sizeof(file_path));
    strcat(file_path, TIB_CONFIG_DIR);
    strcat(file_path, rsu_name);
    strcat(file_path, TIB_CONFIG_FILENAME);

    fp = fopen(file_path, "r");
    if (fp == NULL) {
        LOG_MSG_FATAL("error opening %s", file_path);
        return TIB_CONFIG_INVALID_OPEN_FILE;
    } else {
        LOG_MSG_INFO("%s opened successfully", file_path);
    }

    char read_buf[CONFIG_LINE_BUFFER_SIZE];
    int int_val;
    uint8_t uint8_t_val;
    double double_val;
    float float_val;
    int TIM_view_angle_start;
    int TIM_view_angle_end;

#define FreeAndReturnInvalid(v, errorType, errMsg)                  \
    do {                                                            \
        vector_free(TIB_config.lane_list);                          \
        vector_free(TIB_config.connectsTo_list);                    \
        vector_free(v);                                             \
        LOG_MSG_FATAL("TIB config error: %s", errMsg); \
        return TIB_CONFIG_##errorType##_INVALID;                    \
    } while (0);

#define TIM_FreeAndReturnInvalid(v, errorType, errMsg)                  \
    do {                                                            \
        vector_free(TIB_config.TIM_table);                          \
        vector_free(v);                                             \
        LOG_MSG_FATAL("TIB config error: %s", errMsg); \
        return TIB_CONFIG_##errorType##_INVALID;                    \
    } while (0);

#define RSA_FreeAndReturnInvalid(v, errorType, errMsg)                  \
    do {                                                            \
        vector_free(TIB_config.RSA_table);                          \
        vector_free(v);                                             \
        LOG_MSG_FATAL("TIB config error: %s", errMsg); \
        return TIB_CONFIG_##errorType##_INVALID;                    \
    } while (0);



    while (!feof(fp)) {
        char *buf = read_line(read_buf, sizeof(read_buf), fp);
        if (buf == NULL)
            continue;

        // MAP_packet_transfer_speed
        if (strstr(buf, "MAP_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.MAP_packet_transfer_speed = uint8_t_val;
                    LOG_MSG_INFO("config: MAP_packet_transfer_speed = %d", TIB_config.MAP_packet_transfer_speed);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
                }
            } else {
                return TIB_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
            }
        }

        // SPaT_packet_transfer_speed
        if (strstr(buf, "SPaT_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.SPaT_packet_transfer_speed = uint8_t_val;
                    LOG_MSG_INFO("config: SPaT_packet_transfer_speed = %d", TIB_config.SPaT_packet_transfer_speed);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED;
                }
            } else {
                return TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED;
            }
        }
                
        if (strstr(buf, "general_packet_time_per_cycle ")) {
            if (read_float_from_config_line(buf, &float_val)) {
                if (float_val >= 0) {
                    TIB_config.general_packet_time_per_cycle = float_val;
                    LOG_MSG_INFO("config: general_packet_time_per_cycle = %f",
                                   TIB_config.general_packet_time_per_cycle);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_GENERAL_PACKET_TIME_PER_CYCLE;
                }
            } else {
                return TIB_CONFIG_INVALID_GENERAL_PACKET_TIME_PER_CYCLE;
            }
        }

        if (strstr(buf, "MAP_packet_cycle_per_transfer ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.MAP_packet_cycle_per_transfer = uint8_t_val;
                    LOG_MSG_INFO("config: MAP_packet_cycle_per_transfer = %d", TIB_config.MAP_packet_cycle_per_transfer);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_MAP_PACKET_CYCLE_PER_TRANSFER;
                }
            } else {
                return TIB_CONFIG_INVALID_MAP_PACKET_CYCLE_PER_TRANSFER;
            }
        }

        if (strstr(buf, "SPaT_packet_cycle_per_transfer ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.SPaT_packet_cycle_per_transfer = uint8_t_val;
                    LOG_MSG_INFO("config: SPaT_packet_cycle_per_transfer = %d", TIB_config.SPaT_packet_cycle_per_transfer);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_SPAT_PACKET_CYCLE_PER_TRANSFER;
                }
            } else {
                return TIB_CONFIG_INVALID_SPAT_PACKET_CYCLE_PER_TRANSFER;
            }
        }
        
        if (strstr(buf, "TIM_packet_cycle_per_transfer ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.TIM_packet_cycle_per_transfer = uint8_t_val;
                    LOG_MSG_INFO("config: TIM_packet_cycle_per_transfer = %d", TIB_config.TIM_packet_cycle_per_transfer);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_TIM_PACKET_CYCLE_PER_TRANSFER;
                }
            } else {
                return TIB_CONFIG_INVALID_TIM_PACKET_CYCLE_PER_TRANSFER;
            }
        }

        if (strstr(buf, "RSA_packet_cycle_per_transfer ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.RSA_packet_cycle_per_transfer = uint8_t_val;
                    LOG_MSG_INFO("config: RSA_packet_cycle_per_transfer = %d", TIB_config.RSA_packet_cycle_per_transfer);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_RSA_PACKET_CYCLE_PER_TRANSFER;
                }
            } else {
                return TIB_CONFIG_INVALID_RSA_PACKET_CYCLE_PER_TRANSFER;
            }
        }

        
        // TIB_dontSend2TC
        if (strstr(buf, "TIB_dontSend2TC ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.TIB_dontSend2TC = uint8_t_val;
                    LOG_MSG_INFO("config: TIB_dontSend2TC = %d",
                                   TIB_config.TIB_dontSend2TC);
                    continue;
                } else {
                    return -1;
                }
            } else {
                return -1;
            }
        }

        // LaneSet_table
        if (strstr(buf, "LaneSet_table_start")) {
            const char delim[] = ",";

            vector_init(TIB_config.lane_list);
            for (int i = 0; i < COMPASS_NUM; i++) {
                INIT_LIST_HEAD(&TIB_config.MAP_lane_approach[i]);
                INIT_LIST_HEAD(&TIB_config.MAP_sidewalk_approach[i]);
            }

            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_LaneSet_table_INVALID;

                if (strstr(buf, "LaneSet_table_end")) {
                    break;
                }

                vector_t(char *) str_arr, str_arr_tmp;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);
                MAP_config_lane_t config_lane = {0};

                // laneId ~ node_count
                if (str_arr.size < 6)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table element err");

                int index = 0;
                // laneId
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table laneId err");
                if (int_val != TIB_config.lane_list.size)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table laneId err");
                config_lane.config_laneID = int_val;

                // lane_direction
                substr = vector_at(str_arr, index++);
                if (strstr(substr, "egress")) {
                    config_lane.direction |= 1 << LaneDirection_egressPath;
                } else if (strstr(substr, "ingress")) {
                    config_lane.direction |= 1 << LaneDirection_ingressPath;
                } else if (strstr(substr, "both")) {
                    config_lane.direction |= 1 << LaneDirection_egressPath;
                    config_lane.direction |= 1 << LaneDirection_ingressPath;
                } else {
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table lane_direction err");
                }

                // ApproachId
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.approach) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table ApproachId err");
                config_lane.approach--;  // 因為 array 從 0 開始數

                // lane_index
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.lane_index) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table lane_index err");

                // SharedWith
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                for (int i = 0; i < str_arr_tmp.size; i++) {
                    substr = vector_at(str_arr_tmp, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                        vector_free(str_arr_tmp);
                        FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table SharedWith err");
                    }
                    config_lane.shared_with |= 1 << uint8_t_val;
                }
                vector_free(str_arr_tmp);

                // LaneType
                substr = vector_at(str_arr, index++);
                if (strstr(substr, "vehicle")) {
                    config_lane.lane_type = LaneTypeAttributes_vehicle;
                } else if (strstr(substr, "crosswalk")) {
                    config_lane.lane_type = LaneTypeAttributes_crosswalk;
                } else if (strstr(substr, "bikeLane")) {
                    config_lane.lane_type = LaneTypeAttributes_bikeLane;
                } else if (strstr(substr, "sidewalk")) {
                    config_lane.lane_type = LaneTypeAttributes_sidewalk;
                } else if (strstr(substr, "trackedVehicle")) {
                    config_lane.lane_type = LaneTypeAttributes_trackedVehicle;
                } else {
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table LaneType err");
                }

                // LaneAttributes
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                for (int i = 0; i < str_arr_tmp.size; i++) {
                    substr = vector_at(str_arr_tmp, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                        vector_free(str_arr_tmp);
                        FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table LaneAttributes err");
                    }
                    config_lane.lane_attributes |= 1 << uint8_t_val;
                }
                vector_free(str_arr_tmp);

                // node_count
                int node_count;
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table node_count err");

                if (str_arr.size < index + node_count)
                    FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table node_count err");
                vector_init(config_lane.node_list);

                vector_init(str_arr_tmp);
                for (int i = 0; i < node_count; i++) {
                    MAP_Node_t node;
                    substr = vector_at(str_arr, index++);
                    read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                    if (str_arr_tmp.size != 2) {
                        vector_free(str_arr_tmp);
                        vector_free(config_lane.node_list);
                        FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table node err");
                    }

                    substr = vector_at(str_arr_tmp, 0);
                    if (substr == NULL || sscanf(substr, "%lf", &node.lon) != 1) {
                        vector_free(str_arr_tmp);
                        vector_free(config_lane.node_list);
                        FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table node err");
                    }

                    substr = vector_at(str_arr_tmp, 1);
                    if (substr == NULL || sscanf(substr, "%lf", &node.lat) != 1) {
                        vector_free(str_arr_tmp);
                        vector_free(config_lane.node_list);
                        FreeAndReturnInvalid(str_arr, LaneSet_table, "LaneSet_table node err");
                    }
                    vector_push_back(config_lane.node_list, node);
                    str_arr_tmp.size = 0;
                }
                vector_free(str_arr_tmp);
                vector_free(str_arr);
                vector_push_back(TIB_config.lane_list, config_lane);
            }
            for (int i = 0; i < TIB_config.lane_list.size; i++) {
                MAP_config_lane_t *lane = &vector_at(TIB_config.lane_list, i);
                INIT_LIST_HEAD(&(lane->approach_node));
                if (lane->approach < COMPASS_NUM && lane->lane_type == LaneTypeAttributes_vehicle && lane->direction & (1 << LaneDirection_ingressPath)) {
                    list_add_tail(&lane->approach_node, &TIB_config.MAP_lane_approach[lane->approach]);
                } else if (lane->approach < COMPASS_NUM && lane->lane_type == LaneTypeAttributes_sidewalk) {
                    list_add_tail(&lane->approach_node, &TIB_config.MAP_sidewalk_approach[lane->approach]);
                }
            }
        }

        if (strstr(buf, "LaneSet_connectsTo_table_start")) {
            vector_init(TIB_config.connectsTo_list);
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_LaneSet_ConnectsTo_table_INVALID;

                if (strstr(buf, "LaneSet_connectsTo_table_end")) {
                    break;
                }
                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);

                MAP_config_connectsTo_t connectsTo;

                int index = 0;
                if (str_arr.size < index + 2)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table element err");
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table laneID err");
                if (uint8_t_val >= TIB_config.lane_list.size || uint8_t_val < 0)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table laneID err");
                connectsTo.config_laneID = uint8_t_val;

                int connect_count = 0;
                // left
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table left err");
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table left err");

                vector_init(connectsTo.left_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table left_laneId err");
                    }
                    vector_push_back(connectsTo.left_laneId, uint8_t_val);
                }
                index += connect_count;

                // straight
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table straight err");
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table straight err");

                vector_init(connectsTo.straight_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        vector_free(connectsTo.straight_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table straight_laneId err");
                    }
                    vector_push_back(connectsTo.straight_laneId, uint8_t_val);
                }
                index += connect_count;

                // right
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table right err");
                if (connect_count < 0 || str_arr.size < index + connect_count)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table right err");

                vector_init(connectsTo.right_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        vector_free(connectsTo.straight_laneId);
                        vector_free(connectsTo.right_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table, "LaneSet_ConnectsTo_table right_laneId err");
                    }
                    vector_push_back(connectsTo.right_laneId, uint8_t_val);
                }
                vector_free(str_arr);
                vector_push_back(TIB_config.connectsTo_list, connectsTo);
            }
        }

        if (strstr(buf, "SignalGroupID_table_start")) {
            memset(TIB_config.signalGroupId_table, -1, sizeof(TIB_config.signalGroupId_table));
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_SignalGroupID_table_INVALID;

                if (strstr(buf, "SignalGroupID_table_end")) {
                    break;
                }
                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);
                // SignalGroupID ~ SignalGreenType
                if (str_arr.size < 3)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table element err");

                int index = 0;
                int16_t signalGroupId = -1, approachID, SignalGreenType, SignalID, MapGreenType;
                signalID_obj_t sGobj = {0};
                // SignalGroupID
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hd", &signalGroupId) != 1 || signalGroupId == -1)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table SignalGroupID err");

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hd", &approachID) != 1 || approachID >= COMPASS_NUM)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table approachID err");
                approachID--;  // 因為 array 從 0 開始數

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hd", &SignalGreenType) != 1 || SignalGreenType > 4)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table SignalGreenType err");

                // 已經有重複的
                if (TIB_config.signalGroupId_table[approachID][SignalGreenType] != -1)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table signalGroupId_table err");
                TIB_config.signalGroupId_table[approachID][SignalGreenType] = signalGroupId;

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hd", &SignalID) != 1 || SignalID > COMPASS_NUM)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table SignalID err");

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hd", &MapGreenType) != 1 || MapGreenType > 4)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table, "SignalGroupID_table MapGreenType err");
                sGobj.signalGroupID = signalGroupId;
                sGobj.approachId = approachID;
                sGobj.signalGreenType = SignalGreenType;

                vector_push_back(TIB_config.signalId_table[SignalID - 1][MapGreenType], sGobj);
                vector_free(str_arr);
            }
        }

        if (strstr(buf, "TIM_table_start")) {
            const char delim_TIM[] = ",";
            vector_init(TIB_config.TIM_table);
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_TIM_table_INVALID;

                if (strstr(buf, "TIM_table_end")) {
                    break;
                }
                vector_t(char *) str_arr, str_arr_tmp;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);
                TIM_config_sign_t TIM_signs = {0};

                if (str_arr.size < 6)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table element err");
                
                //TimMsgID
                int index = 0;
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table TimMsgID err");
                //printf("current TIM_table size: %ld\n",TIB_config.TIM_table.size);
                if (int_val != TIB_config.TIM_table.size)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table TimMsgID size err");
                TIM_signs.TimMsgID = int_val;
                LOG_MSG_INFO("current TIM sign ID as %d\n",TIM_signs.TimMsgID);
                
                //FrameType
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table FrameType err");
                TIM_signs.FrameType = int_val;
                LOG_MSG_INFO("set frametype as %d\n",TIM_signs.FrameType);
                
                //EventLocation
                substr = vector_at(str_arr, index++);
                vector_init(TIM_signs.TimPosition);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                TIM_Position_Node_t node_pos;
                if (str_arr_tmp.size != 2) {
                    vector_free(str_arr_tmp);
                    vector_free(TIM_signs.TimPosition);
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table PositionNode err");
                }

                substr = vector_at(str_arr_tmp, 0);
                LOG_MSG_INFO("node pos lon: %s",substr);
                if (substr == NULL || sscanf(substr, "%lf", &node_pos.lon) != 1) {
                    vector_free(str_arr_tmp);
                    vector_free(TIM_signs.TimPosition);
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table PositionNode lon err");
                }
                //LOG_MSG_INFO("set TIM sign %d node pos lon as %lf\n", TIM_signs.TimMsgID, node_pos.lon);
                
                substr = vector_at(str_arr_tmp, 1);
                LOG_MSG_INFO("node pos lat: %s",substr);
                if (substr == NULL || sscanf(substr, "%lf", &node_pos.lat) != 1) {
                    vector_free(str_arr_tmp);
                    vector_free(TIM_signs.TimPosition);
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table positionNode lat err");
                }
                //LOG_MSG_INFO("set TIM sign %d node pos lat as %lf\n", TIM_signs.TimMsgID, node_pos.lat);
                
                vector_push_back(TIM_signs.TimPosition, node_pos);
                str_arr_tmp.size = 0;
                vector_free(str_arr_tmp);

                //ViewAngle
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                if (str_arr_tmp.size > 2) {
                    vector_free(str_arr_tmp);
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table ViewAngle err");
                }
                for (int i = 0; i < str_arr_tmp.size; i++) {
                    substr = vector_at(str_arr_tmp, i);
                    //LOG_MSG_INFO("viewAngle %d as %s\n", i, substr);
                    if (substr == NULL || sscanf(substr, "%d", &int_val) != 1) {
                        vector_free(str_arr_tmp);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table ViewAngle err");
                    }
                    if (i<2)
                    {
                        TIM_signs.viewAngle[i] = int_val;
                        LOG_MSG_INFO("set TIM sign %d viewAngle %d as %d\n",TIM_signs.TimMsgID, i, TIM_signs.viewAngle[i]);
                    }                    
                }
                str_arr_tmp.size = 0;
                vector_free(str_arr_tmp);

                // Anchor
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                TIM_Position_Node_t anchor_pos;
                if (str_arr_tmp.size != 2) {
                    vector_free(str_arr_tmp);
                }

                substr = vector_at(str_arr_tmp, 0);
                LOG_MSG_INFO("anchor node pos lon: %s",substr);
                if (substr == NULL || sscanf(substr, "%lf", &anchor_pos.lon) != 1) {
                    vector_free(str_arr_tmp);
                }
                LOG_MSG_INFO("set TIM sign %d anchor node pos lon as %lf\n", TIM_signs.TimMsgID, anchor_pos.lon);
                TIM_signs.anchor[0] = anchor_pos.lon;

                substr = vector_at(str_arr_tmp, 1);
                LOG_MSG_INFO("anchor node pos lat: %s",substr);
                if (substr == NULL || sscanf(substr, "%lf", &anchor_pos.lat) != 1) {
                    vector_free(str_arr_tmp);
                    //vector_free(TIM_signs.TimPosition);
                    //TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table AnchorNode lat err");
                }
                LOG_MSG_INFO("set TIM sign %d node pos lat as %lf\n", TIM_signs.TimMsgID, anchor_pos.lat);
                TIM_signs.anchor[1] = anchor_pos.lat;

                str_arr_tmp.size = 0;
                vector_free(str_arr_tmp);

                //ViewPath Broadcast Directionality
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table Directionality err");
                //printf("current TIM_table size: %ld\n",TIB_config.TIM_table.size);
                TIM_signs.directionality = int_val;
                LOG_MSG_INFO("current TIM sign directionality as %d\n",TIM_signs.directionality);

                //ViewPath Broadcast Direction
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                for (int i = 0; i < str_arr_tmp.size; i++) {
                    substr = vector_at(str_arr_tmp, i);
                    //LOG_MSG_INFO("BroadcastDirection %d as %s\n", i, substr);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                        vector_free(str_arr_tmp);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table BroadcastDirection err");
                    }
                    if (i<2)
                    {
                        TIM_signs.BroadcastDirection[i] = uint8_t_val;
                        LOG_MSG_INFO("set TIM sign %d BroadcastDirection %d as %d\n", TIM_signs.TimMsgID, i, TIM_signs.BroadcastDirection[i]);
                    }                    
                }
                vector_free(str_arr_tmp);

                // ViewPath Node count
                int node_count;
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table node_count err");
                LOG_MSG_INFO("set TIM sign %d viewpath node count as %d", TIM_signs.TimMsgID, node_count);
                if (str_arr.size < index + node_count){
                    for (int i = 0; i < str_arr.size; i++) {
                        char *s = vector_at(str_arr, i);
                        LOG_MSG_INFO("str_arr[%d] = '%s'", i, s);
                    }
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table node_count size err");
                }
                // int node_count = atoi(vector_at(str_arr, 5));

                // for (int i = 0; i < node_count; ++i) {
                //     double lon = 0, lat = 0;
                //     const char *node_str = vector_at(str_arr, 6 + i);
                //     if (sscanf(node_str, "%lf %lf", &lon, &lat) != 2) {
                //         TIM_FreeAndReturnInvalid(str_arr, TIM_table, "Invalid node lon/lat pair");
                //     }
                //     LOG_MSG_INFO("Parsed Node[%d] lon=%lf, lat=%lf", i, lon, lat);
                //     // 存入你的 node 結構
                // }
                    
                
                //ViewPath Position lat lon
                vector_init(TIM_signs.TimPath);
                vector_init(str_arr_tmp);
                for (int i = 0; i < node_count; i++) {
                    TIM_Path_Node_t path_node;
                    substr = vector_at(str_arr, index++);
                    read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                    if (str_arr_tmp.size != 2) {
                        vector_free(str_arr_tmp);
                        vector_free(TIM_signs.TimPath);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table ViewPath node err");
                    }

                    substr = vector_at(str_arr_tmp, 0);
                    LOG_MSG_INFO ("ViewPath %d lon: %s", i, substr);
                    if (substr == NULL || sscanf(substr, "%lf", &path_node.lon) != 1) {
                        vector_free(str_arr_tmp);
                        vector_free(TIM_signs.TimPath);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table ViewPath node err");
                    }
                    
                    substr = vector_at(str_arr_tmp, 1);
                    LOG_MSG_INFO ("ViewPath %d lat: %s", i, substr);
                    if (substr == NULL || sscanf(substr, "%lf", &path_node.lat) != 1) {
                        vector_free(str_arr_tmp);
                        vector_free(TIM_signs.TimPath);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table ViewPath node err");
                    }
                    
                    vector_push_back(TIM_signs.TimPath, path_node);
                    str_arr_tmp.size = 0;
                    LOG_MSG_INFO("set TIM sign %d ViewPath node %d lon as %lf\n",TIM_signs.TimMsgID, i, path_node.lon);
                    LOG_MSG_INFO("set TIM sign %d ViewPath node %d lat as %lf\n",TIM_signs.TimMsgID, i, path_node.lat);
                    LOG_MSG_INFO("ViewPath vector size: %d\n",TIM_signs.TimPath.size);
                }
                vector_free(str_arr_tmp);

                //EventType
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table EventType err");
                TIM_signs.eventType = int_val;

                //EventDescription
                substr = vector_at(str_arr, index++);
                vector_init(str_arr_tmp);
                read_string_arr_from_config_line(substr, &str_arr_tmp, TIB_FIELD_DELIM);
                for (int i = 0; i < str_arr_tmp.size; i++) {
                    substr = vector_at(str_arr_tmp, i);
                    if (substr == NULL || sscanf(substr, "%d", &int_val) != 1) {
                        vector_free(str_arr_tmp);
                        TIM_FreeAndReturnInvalid(str_arr, TIM_table, "TIM_table EventDescription err");
                    }
                    if (i<8)
                    {
                        TIM_signs.EventDescription[i] = int_val;
                        LOG_MSG_INFO("set TIM sign %d EventDescription %d as %d\n", TIM_signs.TimMsgID, i, TIM_signs.EventDescription[i]);                
                    }
                }
                vector_free(str_arr_tmp);
                vector_free(str_arr);
                vector_push_back(TIB_config.TIM_table, TIM_signs);
            }
        }             
    }
#undef FreeAndReturnInvalid
#undef TIM_FreeAndReturnInvalid
    fclose(fp);
    return TIB_CONFIG_ACCEPT;
}

void print_config_map(MapData *map, char *buf, int buf_len)
{
    // if (map->intersections.count != 1) {
    //     snprintf(buf, buf_len, "intersections.count only can be 1, but is %d", map->intersections.count);
    //     return;
    // }
    LaneList *laneSet = &map->intersections.tab[0].laneSet;
    snprintf(buf, buf_len, "lane_index, laneID, node_index, lat, lon\n");

    for (int i = 0; i < laneSet->count; i++) {
        for (int j = 0; j < laneSet->tab[i].nodeList.u.nodes.count; j++) {
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", i);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", laneSet->tab[i].laneID);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", j);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf, ", laneSet->tab[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lat / 10000000.0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf\n", laneSet->tab[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lon / 10000000.0);
        }
    }
    for (int i = 0; i < laneSet->count; i++) {
        if (laneSet->tab[i].connectsTo_option == FALSE)
            continue;
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", i);
        ConnectsToList *connlist = &laneSet->tab[i].connectsTo;
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", connlist->count);
        for (int k = 0; k < connlist->count; k++) {
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", connlist->tab[k].connectingLane.lane);
        }
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "\n");
    }
    for (int i = 0; i < COMPASS_NUM; i++) {
        MAP_config_lane_t *lane, *safe;
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "Road %d: ", i);
        list_for_each_entry_safe(lane, safe, &TIB_config.MAP_lane_approach[i], approach_node)
        {
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", map->intersections.tab[0].laneSet.tab[lane->config_laneID].laneID);
        }
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "\n");
    }

    LOG_MSG_INFO("Map Config init %s", buf);
    memset(buf, 0, buf_len);

    log_snprintf(buf, "connectsTo_list \n");
    for (int i = 0; i < TIB_config.connectsTo_list.size; i++) {
        MAP_config_connectsTo_t *connectsTo = &vector_at(TIB_config.connectsTo_list, i);
        log_snprintf(buf, "config_laneID: %d\n left_laneId: ", connectsTo->config_laneID);
        for (int j = 0; j < connectsTo->left_laneId.size; j++) {
            log_snprintf(buf, "%d ", vector_at(connectsTo->left_laneId, j));
        }
        log_snprintf(buf, "\n straight_laneId: ");
        for (int j = 0; j < connectsTo->straight_laneId.size; j++) {
            log_snprintf(buf, "%d ", vector_at(connectsTo->straight_laneId, j));
        }
        log_snprintf(buf, "\n right_laneId: ");
        for (int j = 0; j < connectsTo->right_laneId.size; j++) {
            log_snprintf(buf, "%d ", vector_at(connectsTo->right_laneId, j));
        }
        log_snprintf(buf, "\n");
    }
    snprintf(buf + strlen(buf), buf_len - strlen(buf), "signalGroupId_table \n");
    for (int i = 0; i < COMPASS_NUM; i++) {
        for (int j = 0; j < NumOfGreen; j++) {
            log_snprintf(buf, "%d ", TIB_config.signalGroupId_table[i][j]);
        }
        log_snprintf(buf, "\n");
    }
    log_snprintf(buf, "signalId_table \n");
    for (int i = 0; i < COMPASS_NUM; i++) {
        for (int j = 0; j < NumOfGreen; j++) {
            for (int k = 0; k < vector_size(TIB_config.signalId_table[i][j]); k++) {
                signalID_obj_t *obj = &vector_at(TIB_config.signalId_table[i][j], k);
                log_snprintf(buf, "%d %d %d %d %d\n", i, j, obj->signalGroupID, obj->approachId, obj->signalGreenType);
            }
        }
    }
    log_snprintf(buf, "\n");
    LOG_MSG_INFO("Map Config init %s", buf);
}

void print_config_tim(TravelerInformation *tim, char *buf, int buf_len) 
{
    printf("amount of TIM dataframes: %d\n",tim->dataFrames.count);
    snprintf(buf, buf_len, "TimMsgID, FrameType, EventLocation, ViewAngle, Anchor, Directionality, BroadcastDirection, NodeCount, NodeLon NodeLat, ..., EventType, EventDescription\n");
    for (int i = 0; i < tim->dataFrames.count; i++) 
    {
        TIM_config_sign_t *TIM_signs = &vector_at(TIB_config.TIM_table, i);
        TravelerDataFrame *tdf = &tim->dataFrames.tab[i];
        printf("nowtime : %d\n", tim->timeStamp);
        printf("priority : %d\n", tdf->priority);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", i);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", tdf->frameType);
        GeographicalPath *geo = tdf->regions.tab;
        printf("geo anchor: %0.7f %0.7f", TIM_signs->anchor[0], TIM_signs->anchor[1]);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%0.7f ", tdf->msgId.u.roadSignID.position.Long/10000000.0);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%0.7f, ", tdf->msgId.u.roadSignID.position.lat/10000000.0);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d %d, ", TIM_signs->viewAngle[0], TIM_signs->viewAngle[1]);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%0.7f ", TIM_signs->anchor[0]);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%0.7f, ", TIM_signs->anchor[1]);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", TIM_signs->directionality);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d %d, ", TIM_signs->BroadcastDirection[0], TIM_signs->BroadcastDirection[1]);
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%ld, ", TIM_signs->TimPath.size);
        NodeSetXY nodes = geo->description.u.path.offset.u.xy.u.nodes;
        for (int a=0; a<TIM_signs->TimPath.size; a++) 
        {
            TIM_Path_Node_t node_pos = vector_at(TIM_signs->TimPath, a);
            printf("TIM path %d node pos: %lf %lf \n", a, node_pos.lon, node_pos.lat);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf ", node_pos.lon);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf, ", node_pos.lat);
        }
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", tdf->content.choice);
        if (tdf->content.choice==TDFcontent_genericSign) {
            int count = tdf->content.u.genericSign.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.genericSign.tab[i].u.itis!=0)
                {
                    printf("TIM Sign ITIS code %d: %d\n",i,tdf->content.u.genericSign.tab[i].u.itis);
                    snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", tdf->content.u.genericSign.tab[i].u.itis);
                }
                
            }
        }
        else if (tdf->content.choice==TDFcontent_workZone) {
            int count = tdf->content.u.workZone.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.genericSign.tab[i].u.itis!=0)
                {
                    printf("TIM workZone ITIS code %d: %d\n",i,tdf->content.u.workZone.tab[i].u.itis);
                    snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", tdf->content.u.workZone.tab[i].u.itis);
                }                
            }
        }
        else if (tdf->content.choice==TDFcontent_speedLimit) {
            int count = tdf->content.u.speedLimit.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.speedLimit.tab[i].u.itis!=0)
                {
                    snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", tdf->content.u.speedLimit.tab[i].u.itis);
                }                
            }
        }
        else if (tdf->content.choice==TDFcontent_exitService) {
            int count = tdf->content.u.exitService.count;
            for (int i=0;i<count;i++) {
                if (tdf->content.u.exitService.tab[i].u.itis!=0)
                {
                    snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", tdf->content.u.exitService.tab[i].u.itis);
                }                
            }
        }
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "\n");        
    }
    LOG_MSG_INFO("TIM Config init %s", buf);
}