#include "MAP_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

MAP_config_object_t MAP_config = {
    .MAP_packet_transfer_speed = 1,
    .MAP_dontSend2TC = 1,
    .lane_list = {0},
    .connectsTo_list = {0},
};

int MAP_config_init()
{
    FILE *fp;
    fp = fopen(MAP_CONFIG_FILE, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", MAP_CONFIG_FILE);
        return MAP_CONFIG_INVALID_OPEN_FILE;
    } else {
        log_file_write("%s opened successfully", MAP_CONFIG_FILE);
    }

    char read_buf[CONFIG_LINE_BUFFER_SIZE];
    int int_val;
    uint8_t uint8_t_val;
    double double_val;

#define FreeAndReturnInvalid(v)                  \
    do {                                         \
        vector_free(MAP_config.lane_list);       \
        vector_free(MAP_config.connectsTo_list); \
        vector_free(v);                          \
        return MAP_CONFIG_INVALID;               \
    } while (0);

    while (!feof(fp)) {
        char *buf = read_line(read_buf, sizeof(read_buf), fp);
        if (buf == NULL)
            continue;

        // MAP_packet_transfer_speed
        if (strstr(buf, "MAP_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    MAP_config.MAP_packet_transfer_speed = uint8_t_val;
                    log_file_write("config: MAP_packet_transfer_speed = %d", MAP_config.MAP_packet_transfer_speed);
                    continue;
                } else {
                    return MAP_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
                }
            } else {
                return MAP_CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
            }
        }

        // MAP_dontSend2TC
        if (strstr(buf, "MAP_dontSend2TC ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
                    MAP_config.MAP_dontSend2TC = uint8_t_val;
                    log_file_write("config: MAP_dontSend2TC = %d",
                                   MAP_config.MAP_dontSend2TC);
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
            vector_init(MAP_config.lane_list);
            const char delim[] = ",";

            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return MAP_CONFIG_INVALID;

                if (strstr(buf, "LaneSet_table_end")) {
                    break;
                }

                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, ",");
                MAP_config_lane_t config_lane;

                // laneId ~ node_count
                if (str_arr.size < 5)
                    FreeAndReturnInvalid(str_arr);

                // laneId
                char *substr = vector_at(str_arr, 0);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (int_val != MAP_config.lane_list.size)
                    FreeAndReturnInvalid(str_arr);

                // direction
                substr = vector_at(str_arr, 1);

                if (strstr(substr, "egress")) {
                    config_lane.direction = LaneDirection_egressPath;
                } else if (strstr(substr, "ingress")) {
                    config_lane.direction = LaneDirection_ingressPath;
                } else {
                    FreeAndReturnInvalid(str_arr);
                }

                substr = vector_at(str_arr, 2);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.approach) != 1)
                    FreeAndReturnInvalid(str_arr);

                // lane_index
                substr = vector_at(str_arr, 3);
                if (substr == NULL || sscanf(substr, "%hhd", &config_lane.lane_index) != 1)
                    FreeAndReturnInvalid(str_arr);

                // node_count
                int node_count;
                substr = vector_at(str_arr, 4);
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    FreeAndReturnInvalid(str_arr);

                if (str_arr.size < 5 + (node_count * 2))
                    FreeAndReturnInvalid(str_arr);

                for (int i = 0; i < node_count; i++) {
                    MAP_Node_t node;
                    substr = vector_at(str_arr, 5 + (i * 2));
                    if (substr == NULL || sscanf(substr, "%lf", &node.lat) != 1)
                        FreeAndReturnInvalid(str_arr);

                    substr = vector_at(str_arr, 5 + (i * 2) + 1);
                    if (substr == NULL || sscanf(substr, "%lf", &node.lon) != 1)
                        FreeAndReturnInvalid(str_arr);
                    vector_push_back(config_lane.node_list, node);
                }
                vector_free(str_arr);
            }
        }

        if (strstr(buf, "LaneSet_ConnectsTo_table_start")) {
            vector_init(MAP_config.connectsTo_list);
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return MAP_CONFIG_INVALID;

                if (strstr(buf, "LaneSet_ConnectsTo_table_end")) {
                    break;
                }
                vector_t(char *) str_arr;
                vector_init(str_arr);
                read_string_arr_from_config_line(buf, &str_arr, ",");

                MAP_config_connectsTo_t connectsTo;

                int index = 0;
                if (str_arr.size < index + 2)
                    FreeAndReturnInvalid(str_arr);
                char *substr = vector_at(str_arr, index);
                index++;
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (uint8_t_val >= MAP_config.lane_list.size || uint8_t_val < 0)
                    FreeAndReturnInvalid(str_arr);
                connectsTo.config_laneID = uint8_t_val;

                int connect_count = 0;
                // left
                substr = vector_at(str_arr, index);
                index++;
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                        FreeAndReturnInvalid(str_arr);
                    if (uint8_t_val >= MAP_config.lane_list.size)
                        FreeAndReturnInvalid(str_arr);
                    vector_push_back(connectsTo.left_laneId, uint8_t_val);
                }
                index += connect_count;

                // stright
                substr = vector_at(str_arr, index);
                index++;
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (connect_count < 0 || str_arr.size < index + connect_count + 1)
                    FreeAndReturnInvalid(str_arr);

                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                        FreeAndReturnInvalid(str_arr);
                    if (uint8_t_val >= MAP_config.lane_list.size)
                        FreeAndReturnInvalid(str_arr);
                    vector_push_back(connectsTo.stright_laneId, uint8_t_val);
                }
                index += connect_count;

                // right
                substr = vector_at(str_arr, index);
                index++;
                if (substr == NULL || sscanf(substr, "%d", &connect_count) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (connect_count < 0 || str_arr.size < index + connect_count)
                    FreeAndReturnInvalid(str_arr);
                for (int i = index; i < index + connect_count; i++) {
                    substr = vector_at(str_arr, i);
                    if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                        FreeAndReturnInvalid(str_arr);
                    if (uint8_t_val >= MAP_config.lane_list.size)
                        FreeAndReturnInvalid(str_arr);
                    vector_push_back(connectsTo.right_laneId, uint8_t_val);
                }
                vector_free(str_arr);
            }
        }
    }
#undef FreeAndReturnInvalid
    fclose(fp);
    return MAP_CONFIG_ACCEPT;
}

void print_config_map(MapData *map, char *buf, int buf_len)
{
    if (map->intersections.count != 1) {
        snprintf(buf, buf_len, "intersections.count only can be 1, but is %d", map->intersections.count);
        return;
    }
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
    printf("\n");
}