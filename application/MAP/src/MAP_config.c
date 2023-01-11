#include "MAP_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
#include "config.h"

MAP_config_object_t MAP_config = {
    .MAP_packet_transfer_speed = 1,
    .MAP_dontSend2TC = 1,
};

static bool read_float_from_config_line(char *config_line, float *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %f\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}
static bool read_int_from_config_line(char *config_line, int *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    *val = 0;
    if (sscanf(config_line, "%s %d\n", prm_name, val) == 2) {
        return true;
    } else {
        return false;
    }
}

static bool read_int_array_from_config_line(char *config_line, int *val)
{
    char prm_name[MAX_CONFIG_VARIABLE_LEN];
    memset(val, 0, sizeof(int) * LANE_MAX_NUMBER);
    if (sscanf(config_line, "%s %d %d %d %d %d\n", prm_name, &val[0],
               &val[1], &val[2], &val[3], &val[4]) == 6) {
        return true;
    } else {
        return false;
    }
}

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
    float float_val;
    double double_val;
    char string_val[MAX_CONFIG_VARIABLE_LEN];
    int int_val_array[LANE_MAX_NUMBER];
    int intersection_n = 0;

    int intersection_number;
    int intersection_count = 1;
    int intersection_laneSet_number = 0;
    int intersection_laneSet_count = 1;
    int intersection_speedLimits_count = 1;
    int intersection_speedLimits_number = 0;
    int intersection_laneSet_NodeXY_count = 1;
    int intersection_laneSet_NodeXY_number = 0;
    int intersection_connectsTo_number = 0;
    int intersection_connectsTo_n = 0;
    int direction_index = 0;
    int Lane_index = 0;
    MAP_config.Mapconfig = (MapData *) j2735_msg_prealloc(MapData_Id);
    MAP_config.Mapconfig->msgIssueRevision = 0;
    // MAP_config.intersections.tab = (IntersectionGeometry *) calloc(1, sizeof(IntersectionGeometry));
    // MAP_config.intersections.tab->speedLimits.tab = (RegulatorySpeedLimit *) calloc(1, sizeof(RegulatorySpeedLimit));
    // MAP_config.intersections.tab->laneSet.tab = (GenericLane *) calloc(1, sizeof(GenericLane));
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

        // LaneSet_table
        if (strstr(buf, "LaneSet_table_start")) {
            IntersectionGeometry *intersection = &MAP_config.Mapconfig->intersections.tab[0];

            const char const delim[] = ",";
            while (!feof(fp)) {
                buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    return MAP_CONFIG_INVALID;

                if (strstr(buf, "LaneSet_table_end")) {
                    break;
                }

                char *save_ptr = NULL;
                // laneId
                char *substr = trim_space(strtok_r(buf, delim, &save_ptr));
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    return MAP_CONFIG_INVALID;
                if (int_val != intersection->laneSet.count)
                    return MAP_CONFIG_INVALID;

                GenericLane *lane = &intersection->laneSet.tab[intersection->laneSet.count];

                // Approach
                substr = trim_space(strtok_r(NULL, delim, &save_ptr));
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
                    return MAP_CONFIG_INVALID;
                }

                // Approach
                substr = trim_space(strtok_r(NULL, delim, &save_ptr));
                if (substr == NULL || sscanf(substr, "%d", ApproachId) != 1)
                    return MAP_CONFIG_INVALID;

                // lane_index
                substr = trim_space(strtok_r(NULL, delim, &save_ptr));
                if (substr == NULL || sscanf(substr, "%d", &int_val) != 1)
                    return MAP_CONFIG_INVALID;

                // bit 5-8 為 Approach
                lane->laneID = ((*ApproachId << 5) & 0b11100000);

                // ingress 設 bit 4 為 0, egress 為 0
                if (asn1_bstr_is_bit_set(&lane->laneAttributes.directionalUse, LaneDirection_ingressPath)) {
                    lane->laneID |= 0b00010000;
                }

                lane->laneID |= (0b00001111 & int_val);

                // node_count
                int node_count;
                substr = trim_space(strtok_r(NULL, delim, &save_ptr));
                if (substr == NULL || sscanf(substr, "%d", &node_count) != 1)
                    return MAP_CONFIG_INVALID;

                lane->nodeList.choice = NodeListXY_nodes;

                Malloc(lane->nodeList.u.nodes.tab, sizeof(NodeXY) * node_count, "MAP_config_NodeXY_new");

                for (int i = 0; i < node_count; i++, lane->nodeList.u.nodes.count++) {
                    lane->nodeList.u.nodes.tab[i].delta.choice = NodeOffsetPointXY_node_LatLon;

                    substr = trim_space(strtok_r(NULL, delim, &save_ptr));
                    if (substr == NULL || sscanf(substr, "%lf", &double_val) != 1)
                        return MAP_CONFIG_INVALID;
                    lane->nodeList.u.nodes.tab[i].delta.u.node_LatLon.lat = double_val * 10000000;

                    substr = trim_space(strtok_r(NULL, delim, &save_ptr));

                    if (substr == NULL || sscanf(substr, "%lf", &double_val) != 1)
                        return MAP_CONFIG_INVALID;
                    lane->nodeList.u.nodes.tab[i].delta.u.node_LatLon.lon = double_val * 10000000;
                }
                intersection->laneSet.count++;
            }
            MAP_config.Mapconfig->intersections.count = 1;
        }

        if (strstr(buf, "intersection_connectsTo_connectingLane_maneuver ")) {
            if (read_int_from_config_line(buf, &int_val)) {
            }
        }

        // Direction
        if (strstr(buf, "Direction ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                direction_index = uint8_t_val;
            }
        }

        // Lane_count
        if (strstr(buf, "Lane_count ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                MAP_config.map_lane2connecting.Direction[direction_index].Lane_count = uint8_t_val;
            }
        }

        // Lane_Index
        if (strstr(buf, "Lane_Index ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                Lane_index = uint8_t_val;
            }
        }

        // LaneID
        if (strstr(buf, "LaneID ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                MAP_config.map_lane2connecting.Direction[direction_index].connectingLane[Lane_index].LaneID = uint8_t_val;
            }
        }

        // LeftconnectingLane
        if (strstr(buf, "LeftconnectingLane ")) {
            if (read_int_array_from_config_line(buf, int_val_array)) {
                for (int i = 0; i < LANE_MAX_NUMBER; i++) {
                    if (int_val_array[i] > 0) {
                        MAP_config.map_lane2connecting.Direction[direction_index].connectingLane[Lane_index].LeftconnectingLane[i] = int_val_array[i];
                    }
                }
            }
        }

        // StrightconnectingLane
        if (strstr(buf, "StrightconnectingLane ")) {
            if (read_int_array_from_config_line(buf, int_val_array)) {
                for (int i = 0; i < LANE_MAX_NUMBER; i++) {
                    if (int_val_array[i] > 0) {
                        MAP_config.map_lane2connecting.Direction[direction_index].connectingLane[Lane_index].StrightconnectingLane[i] = int_val_array[i];
                    }
                }
            }
        }

        // RightconnectingLane
        if (strstr(buf, "RightconnectingLane ")) {
            if (read_int_array_from_config_line(buf, int_val_array)) {
                for (int i = 0; i < LANE_MAX_NUMBER; i++) {
                    if (int_val_array[i] > 0) {
                        MAP_config.map_lane2connecting.Direction[direction_index].connectingLane[Lane_index].RightconnectingLane[i] = int_val_array[i];
                    }
                }
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
        for ( int j = 0 ;j < laneSet->tab[i].nodeList.u.nodes.count; j++) {
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", i);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", laneSet->tab[i].laneID);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%d, ", j);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf, ", laneSet->tab[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lat / 10000000.0);
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%lf\n", laneSet->tab[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lon / 10000000.0);
        }
    }
}