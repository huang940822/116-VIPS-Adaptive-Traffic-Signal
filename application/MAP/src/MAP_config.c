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
    .Mapconfig = NULL,
};

int MAP_config_init()
{
    if (MAP_config.Mapconfig)
        j2735_msg_dealloc(MapData_Id, MAP_config.Mapconfig);
    MAP_config.Mapconfig = (MapData *) j2735_msg_prealloc(MapData_Id);

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

    vector_t(int) tableId_to_laneId;
    vector_init(tableId_to_laneId);

    MAP_config.Mapconfig->msgIssueRevision = 0;

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

#define FreeAndReturnInvalid(v)    \
    do {                           \
        vector_free(v);            \
        return MAP_CONFIG_INVALID; \
    } while (0);

        // LaneSet_table
        if (strstr(buf, "LaneSet_table_start")) {
            IntersectionGeometry *intersection = &MAP_config.Mapconfig->intersections.tab[0];
            intersection->id.id = config.RSU_id;
            intersection->id.region_option = TRUE;
            intersection->id.region = config.RSU_region;

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

                // laneId ~ node_count
                if (str_arr.size < 5)
                    FreeAndReturnInvalid(str_arr);

                // laneId
                char *substr = vector_at(str_arr, 0);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    FreeAndReturnInvalid(str_arr);
                if (int_val != intersection->laneSet.count)
                    FreeAndReturnInvalid(str_arr);

                GenericLane *lane = &intersection->laneSet.tab[intersection->laneSet.count];

                // Approach
                substr = vector_at(str_arr, 1);
                int32_t *ApproachId;

                if (strstr(substr, "egress")) {
                    lane->ingressApproach_option = FALSE;
                    lane->egressApproach_option = TRUE;
                    ApproachId = &lane->egressApproach;
                    asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_egressPath);
                } else if (strstr(substr, "ingress")) {
                    lane->ingressApproach_option = TRUE;
                    lane->egressApproach_option = FALSE;
                    ApproachId = &lane->ingressApproach;
                    asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_ingressPath);
                } else {
                    FreeAndReturnInvalid(str_arr);
                }

                substr = vector_at(str_arr, 2);
                if (substr == NULL || sscanf(substr, "%d", ApproachId) != 1)
                    FreeAndReturnInvalid(str_arr);

                // bit 5-8 為 Approach
                lane->laneID = ((*ApproachId << 5) & 0b11100000);

                // ingress 設 bit 4 為 0, egress 為 0
                if (asn1_bstr_is_bit_set(&lane->laneAttributes.directionalUse, LaneDirection_ingressPath)) {
                    lane->laneID |= 0b00010000;
                }

                // lane_index
                substr = vector_at(str_arr, 3);
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    FreeAndReturnInvalid(str_arr);

                lane->laneID |= (0b00001111 & int_val);

                // node_count
                int node_count;
                substr = vector_at(str_arr, 4);
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    FreeAndReturnInvalid(str_arr);

                lane->nodeList.choice = NodeListXY_nodes;
                Malloc(lane->nodeList.u.nodes.tab, sizeof(NodeXY) * node_count, "MAP_config_NodeXY_new");

                if (str_arr.size < 5 + (node_count * 2))
                    FreeAndReturnInvalid(str_arr);

                for (int i = 0; i < node_count; i++, lane->nodeList.u.nodes.count++) {
                    lane->nodeList.u.nodes.tab[i].delta.choice = NodeOffsetPointXY_node_LatLon;

                    substr = vector_at(str_arr, 4 + (i * 2));
                    if (substr == NULL || sscanf(substr, "%lf", &double_val) != 1)
                        FreeAndReturnInvalid(str_arr);
                    lane->nodeList.u.nodes.tab[i].delta.u.node_LatLon.lat = double_val * 10000000;

                    substr = vector_at(str_arr, 4 + (i * 2) + 1);
                    if (substr == NULL || sscanf(substr, "%lf", &double_val) != 1)
                        FreeAndReturnInvalid(str_arr);
                    lane->nodeList.u.nodes.tab[i].delta.u.node_LatLon.lon = double_val * 10000000;
                }

                vector_free(str_arr);
                intersection->laneSet.count++;
            }
            MAP_config.Mapconfig->intersections.count = 1;
        }

        if (strstr(buf, "LaneSet_ConnectsTo_table_start")) {
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

                vector_free(str_arr);
            }
        }
#undef FreeAndReturnInvalid
    }

    fclose(fp);
    return MAP_CONFIG_ACCEPT;
}

void print_config_map(char *buf, int buf_len)
{
    if (MAP_config.Mapconfig->intersections.count != 1) {
        snprintf(buf, buf_len, "intersections.count only can be 1, but is %d", MAP_config.Mapconfig->intersections.count);
        return;
    }
    LaneList *laneSet = &MAP_config.Mapconfig->intersections.tab[0].laneSet;
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
}