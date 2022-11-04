#include "MAP_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

MAP_config_object_t MAP_config = {
    .MAP_packet_transfer_speed = 1,
    .MAP_dontSend2TC = 1,
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
    if(fp == NULL) {
        log_file_write_fatal_error("error opening %s", MAP_CONFIG_FILE);
    } else {
        log_file_write("%s opened successfully", MAP_CONFIG_FILE);
    }

    char buf[CONFIG_LINE_BUFFER_SIZE];
    int int_val;
    uint8_t uint8_t_val;
    float float_val;
    char string_val[MAX_CONFIG_VARIABLE_LEN];
    int int_val_array[LANE_MAX_NUMBER];
    int intersection_number=0;
    int intersection_count=1;
    int intersection_laneSet_number=0;
    int intersection_laneSet_count=1;
    int intersection_speedLimits_count=1;
    int intersection_speedLimits_number=0;
    int intersection_laneSet_NodeXY_count=1;
    int intersection_laneSet_NodeXY_number=0;
    int intersection_connectsTo_number=0;
    int intersection_connectsTo_n = 0;
    int direction_index = 0;
    int Lane_index = 0;
    MAP_config.Mapconfig = (MapData *) j2735_msg_prealloc(MapData_Id);
    MAP_config.Mapconfig -> msgIssueRevision = 0 ;
    // MAP_config.intersections.tab = (IntersectionGeometry *) calloc(1, sizeof(IntersectionGeometry));
    // MAP_config.intersections.tab->speedLimits.tab = (RegulatorySpeedLimit *) calloc(1, sizeof(RegulatorySpeedLimit));
    // MAP_config.intersections.tab->laneSet.tab = (GenericLane *) calloc(1, sizeof(GenericLane));
    while (!feof(fp)) {
        fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
        if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
            continue;
        }
        if (strstr(buf, "intersection_count ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.count = int_val;
                    // printf("count is %d\n",MAP_config.intersections.count);
            }
        }
        if (strstr(buf, "intersection_n ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    intersection_number = int_val;
            }
        }
        if (strstr(buf, "intersection_revision ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].revision =
                    int_val;
            }
        }
        if (strstr(buf, "intersection_lat ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].refPoint.lat
                    = int_val;
            }
        }
        if (strstr(buf, "intersection_long ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].refPoint.Long
                    = int_val;
            }
        }
        if (strstr(buf, "intersection_elevation ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].refPoint.elevation=
                    int_val;
            }
        }
        if (strstr(buf, "intersection_laneWidth ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneWidth=
                    int_val;
            }
        }
        if (strstr(buf, "intersection_speedLimits_count ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].speedLimits.count=
                    int_val;
            }
        }
        if (strstr(buf, "intersection_speedLimits_n ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    intersection_speedLimits_number= int_val;
            }
        }
        if (strstr(buf, "IntersectionGeometry_speedLimits_type ")) {
            if (read_string_from_config_line(buf, string_val)) {

                // strcpy(MAP_config.Mapconfig->intersections.tab[intersection_number].speedLimits.tab[intersection_speedLimits_number].type, string_val);
            }
        }
        if (strstr(buf, "intersection_speedLimits_speed ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].speedLimits.tab[intersection_speedLimits_number].speed = int_val;
                    // printf("speed = %d\n",MAP_config.Mapconfig->intersections.tab[intersection_number].speedLimits.tab[intersection_speedLimits_number].speed);
            }
        }
        if (strstr(buf, "intersection_laneSet_count ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.count = int_val;
            }
        }
        if (strstr(buf, "intersection_laneSet_n ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    intersection_laneSet_number= int_val;
            }
        }
        if (strstr(buf, "intersection_laneSet_id ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneID = int_val;
                    // printf("laneid=%d\n",MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneID);
            }
        }
        if (strstr(buf, "intersection_laneSet_ingressApproach ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].ingressApproach=
                    int_val;
            }
        }
        if (strstr(buf, "intersection_laneSet_egressApproach ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].egressApproach=
                    int_val;
            }
        }
        // ?
        if (strstr(buf, "intersection_laneSet_laneAttributes_directionalUse")) {
            if (read_string_from_config_line(buf, string_val)) {
                    asn1_bstr_alloc(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS); 
                    for(int i=0;i<LaneDirection_MAX_BITS;i++){
                        if(string_val[i]=='1')
                            asn1_bstr_set_bit(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.directionalUse),
                            i);
                     
                    }
            }
        }
        // ?
        if (strstr(buf, "intersection_laneSet_laneAttributes_sharedWith ")) {
            if (read_string_from_config_line(buf, string_val)) {
                    asn1_bstr_alloc(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.sharedWith),
                    LaneSharing_MAX_BITS); for(int
                    i=0;i<LaneSharing_MAX_BITS;i++){
                        if(string_val[i]=='1')
                            asn1_bstr_set_bit(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.sharedWith),
                            i);
                    }
            }
        }
        // ?
        if (strstr(buf, "intersection_laneSet_laneAttributes_laneType ")) {
        //VEHICLE ONLY NOW
            if (read_string_from_config_line(buf, string_val)) {
                    asn1_bstr_alloc(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS); for(int
                    i=0;i<LaneAttributes_Vehicle_MAX_BITS;i++){
                        if(string_val[i]=='1')
                            asn1_bstr_set_bit(&(MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.laneType.u.vehicle),
                            i);
                    }
            }
        }
        // ?
        if (strstr(buf, "intersection_laneSet_NodeXY_count ")) {
            //CHOICE SHOULD ADD
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.choice = NodeListXY_nodes;
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.count = int_val;
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab
                    = (NodeXY *)calloc(sizeof(int_val), sizeof(NodeXY));
            }
        }
        if (strstr(buf, "intersection_laneSet_NodeXY_n ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    intersection_laneSet_NodeXY_number= int_val;
                    // MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.choice
                    // = NodeOffsetPointXY_node_XY6;
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.choice
                    = NodeOffsetPointXY_node_LatLon;
            }
        }
        // if (strstr(buf, "intersection_laneSet_NodeXY_X ")) {
        //     if (read_int_from_config_line(buf, &int_val)) {
        //             MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_XY6.x=
        //             int_val;
        //     }
        // }
        // if (strstr(buf, "intersection_laneSet_NodeXY_Y ")) {
        //     if (read_int_from_config_line(buf, &int_val)) {
        //             MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_XY6.y=
        //             int_val;
        //     }
        // }
        if (strstr(buf, "intersection_laneSet_NodeXY_lon ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_LatLon.lon=
                    int_val;
            }
        }
        if (strstr(buf, "intersection_laneSet_NodeXY_lat ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_LatLon.lat=
                    int_val;
            }
        }

        if (strstr(buf, "intersection_connectsTo_count ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].connectsTo.count = int_val;
            }
        }
        if (strstr(buf, "intersection_connectsTo_n ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    intersection_connectsTo_number = int_val;
            }
        }
        if (strstr(buf, "intersection_connectsTo_connectingLane_lane ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                    MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].connectsTo.tab[intersection_connectsTo_number].connectingLane.lane = int_val;
            }
        }
        if (strstr(buf, "intersection_connectsTo_connectingLane_maneuver ")) {
            if (read_int_from_config_line(buf, &int_val)) {
                   
            }
        }
        // if (strstr(buf, "intersection_connectsTo_signalGroup ")) {
        //     if (read_int_from_config_line(buf, &int_val)) {
        //             MAP_config.Mapconfig->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].connectsTo.tab[intersection_connectsTo_number].signalGroup = int_val;
        //     }
        // }
        
        // Direction
        if (strstr(buf, "Direction ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)){
                direction_index = uint8_t_val;
            }
        }

        // Lane_count
        if (strstr(buf, "Lane_count ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)){
                MAP_config.map_lane2connecting.Direction[direction_index].Lane_count = uint8_t_val;
            }
        }

        // Lane_Index
        if (strstr(buf, "Lane_Index ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)){
                Lane_index = uint8_t_val;
            }
        }

        // LaneID
        if (strstr(buf, "LaneID ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)){
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

        //MAP_packet_transfer_speed
        if (strstr(buf, "MAP_packet_transfer_speed ")) {
            if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
                if (uint8_t_val >= 0) {
            
                    MAP_config.MAP_packet_transfer_speed = uint8_t_val;
                    log_file_write("config: MAP_packet_transfer_speed = %d",MAP_config.MAP_packet_transfer_speed);
                    continue;
                } else {
                    return CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
                }
            } else {
                return CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
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