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

TIB_config_object_t TIB_config = {
    .MAP_packet_transfer_speed = 1,
    .SPaT_packet_transfer_speed = 10,
    .TIB_dontSend2TC = 1,
    .lane_list = {0},
    .connectsTo_list = {0},
    .MAP_lane_approach = {0},
};

int TIB_config_init()
{
    FILE *fp;
    fp = fopen(TIB_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", TIB_CONFIG_FILE);
        return TIB_CONFIG_INVALID_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", TIB_CONFIG_FILE);
    }

    char read_buf[CONFIG_LINE_BUFFER_SIZE];
    int int_val;
    uint8_t uint8_t_val;
    double double_val;

#define FreeAndReturnInvalid(v, errorType, errMsg)                  \
    do {                                                            \
        vector_free(TIB_config.lane_list);                          \
        vector_free(TIB_config.connectsTo_list);                    \
        vector_free(v);                                             \
        log_file_write_fatal_error("TIB config error: %s", errMsg); \
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
                    log_file_write("config: MAP_packet_transfer_speed = %d", TIB_config.MAP_packet_transfer_speed);
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
                    log_file_write("config: SPaT_packet_transfer_speed = %d", TIB_config.SPaT_packet_transfer_speed);
                    continue;
                } else {
                    return TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED;
                }
            } else {
                return TIB_CONFIG_INVALID_SPAT_PACKET_TRANSFER_SPEED;
            }
        }

        // TIB_dontSend2TC
        if (strstr(buf, "TIB_dontSend2TC ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    TIB_config.TIB_dontSend2TC = uint8_t_val;
                    log_file_write("config: TIB_dontSend2TC = %d",
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
                int16_t signalGroupId = -1, approachID, SignalGreenType;
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
                vector_free(str_arr);
            }
        }
    }
#undef FreeAndReturnInvalid
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
    snprintf(buf + strlen(buf), buf_len - strlen(buf), "connectsTo_list \n");
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
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", TIB_config.signalGroupId_table[i][j]);
        }
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "\n");
    }
}