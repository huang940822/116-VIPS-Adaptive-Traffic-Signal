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
    .MAP_lane_compass = {0},
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

#define FreeAndReturnInvalid(v, errorType)       \
    do {                                         \
        vector_free(TIB_config.lane_list);       \
        vector_free(TIB_config.connectsTo_list); \
        vector_free(v);                          \
        return TIB_CONFIG_##errorType##_INVALID; \
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
            vector_init(TIB_config.lane_list);
            for (int i = 0; i < COMPASS_NUM; i++)
                INIT_LIST_HEAD(&TIB_config.MAP_lane_compass[i]);
            const char delim[] = ",";

            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_LaneSet_table_INVALID;

                if (strstr(buf, "LaneSet_table_end")) {
                    break;
                }

                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);
                MAP_config_lane_t config_lane = {0};

                // laneId ~ node_count
                if (str_arr.size < 6)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);

                int index = 0;
                // laneId
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);
                if (int_val != TIB_config.lane_list.size)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);
                config_lane.config_laneID = int_val;

                // compasss
                substr = vector_at(str_arr, index++);
                int compass = 0;
                const char *compass_order[] = COMPASS_ORDER;
                for (compass = 0; compass < COMPASS_NUM; compass++) {
                    if (strcmp(compass_order[compass], substr) == 0)
                        break;
                }

                // lane_direction
                substr = vector_at(str_arr, index++);
                if (strstr(substr, "egress")) {
                    config_lane.direction = LaneDirection_egressPath;
                } else if (strstr(substr, "ingress")) {
                    config_lane.direction = LaneDirection_ingressPath;
                } else {
                    FreeAndReturnInvalid(str_arr, LaneSet_table);
                }

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.approach) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);
                // lane_index
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.lane_index) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);

                // node_count
                int node_count;
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_table);

                if (str_arr.size < index + (node_count * 2))
                    FreeAndReturnInvalid(str_arr, LaneSet_table);
                vector_init(config_lane.node_list);
                for (int i = 0; i < node_count; i++) {
                    MAP_Node_t node;
                    substr = vector_at(str_arr, index + (i * 2));
                    if (substr == NULL || sscanf(substr, "%lf", &node.lat) != 1) {
                        vector_free(config_lane.node_list);
                        FreeAndReturnInvalid(str_arr, LaneSet_table);
                    }

                    substr = vector_at(str_arr, index + (i * 2) + 1);
                    if (substr == NULL || sscanf(substr, "%lf", &node.lon) != 1) {
                        vector_free(config_lane.node_list);
                        FreeAndReturnInvalid(str_arr, LaneSet_table);
                    }
                    vector_push_back(config_lane.node_list, node);
                }
                vector_free(str_arr);
                vector_push_back(TIB_config.lane_list, config_lane);
                MAP_config_lane_t *lane = &vector_at(TIB_config.lane_list, TIB_config.lane_list.size - 1);
                INIT_LIST_HEAD(&lane->compass_node);
                if (compass < COMPASS_NUM) {
                    list_add_tail(&lane->compass_node, &TIB_config.MAP_lane_compass[compass]);
                }
            }
        }

        if (strstr(buf, "LaneSet_ConnectsTo_table_start")) {
            vector_init(TIB_config.connectsTo_list);
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return TIB_CONFIG_LaneSet_ConnectsTo_table_INVALID;

                if (strstr(buf, "LaneSet_ConnectsTo_table_end")) {
                    break;
                }
                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, TIB_TABLE_DELIM);

                MAP_config_connectsTo_t connectsTo;

                int index = 0;
                if (str_arr.size < index + 2)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                if (uint8_t_val >= TIB_config.lane_list.size || uint8_t_val < 0)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                connectsTo.config_laneID = uint8_t_val;

                int connect_count = 0;
                // left
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);

                vector_init(connectsTo.left_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                    }
                    vector_push_back(connectsTo.left_laneId, uint8_t_val);
                }
                index += connect_count;

                // stright
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);

                vector_init(connectsTo.stright_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        vector_free(connectsTo.stright_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                    }
                    vector_push_back(connectsTo.stright_laneId, uint8_t_val);
                }
                index += connect_count;

                // right
                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                if (connect_count < 0 || str_arr.size < index + connect_count)
                    FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);

                vector_init(connectsTo.right_laneId);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1 ||
                        uint8_t_val >= TIB_config.lane_list.size) {
                        vector_free(connectsTo.left_laneId);
                        vector_free(connectsTo.stright_laneId);
                        vector_free(connectsTo.right_laneId);
                        FreeAndReturnInvalid(str_arr, LaneSet_ConnectsTo_table);
                    }
                    vector_push_back(connectsTo.right_laneId, uint8_t_val);
                }
                vector_free(str_arr);
                vector_push_back(TIB_config.connectsTo_list, connectsTo);
            }
        }

        if (strstr(buf, "SignalGroupID_table_start")) {
            memset(TIB_config.signalGroupId_table, 0, sizeof(TIB_config.signalGroupId_table));
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
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table);

                int index = 0;
                uint8_t signalGroupId = 0, ingressAngle, SignalGreenType;
                // SignalGroupID
                char *substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &signalGroupId) != 1 || signalGroupId == 0)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table);

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &ingressAngle) != 1 || ingressAngle > 7)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table);

                substr = vector_at(str_arr, index++);
                if (substr == NULL || sscanf(substr, "%hhd", &SignalGreenType) != 1 || SignalGreenType > 4)
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table);

                // 已經有重複的
                if (TIB_config.signalGroupId_table[ingressAngle][SignalGreenType])
                    FreeAndReturnInvalid(str_arr, SignalGroupID_table);
                TIB_config.signalGroupId_table[ingressAngle][SignalGreenType] = signalGroupId;
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
    const char *conpass_order[] = COMPASS_ORDER;
    for (int i = 0; i < COMPASS_NUM; i++) {
        MAP_config_lane_t *lane, *safe;
        snprintf(buf + strlen(buf), buf_len - strlen(buf), "%s: ", conpass_order[i]);
        list_for_each_entry_safe(lane, safe, &TIB_config.MAP_lane_compass[i], compass_node)
        {
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d ", map->intersections.tab[0].laneSet.tab[lane->config_laneID].laneID);
        }
    }
}