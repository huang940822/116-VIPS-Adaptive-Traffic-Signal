
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "log.h"
#include "traffic_signal_status_updating.h"
#include "error_code_user.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "MAP_utils.h"
#include "MAP_config.h"
#include "config.h"

extern MapData *map;

void map_msg_init(MapData *map)
{
    map->timeStamp_option = FALSE;
    map->msgIssueRevision = 0;

    map->intersections_option = TRUE;
    map->intersections.count = 1;

    map->intersections.tab->id.id = config.RSU_id;
    map->intersections.tab->id.region_option = TRUE;
    map->intersections.tab->id.region = config.RSU_region;

    map->intersections.tab->revision = 0;

    map->intersections.tab->refPoint.lat = config.RSU_lat * 10000000;
    map->intersections.tab->refPoint.Long = config.RSU_lon * 10000000;
    map->intersections.tab->refPoint.elevation_option = TRUE;
    map->intersections.tab->refPoint.elevation = config.RSU_elev * 10000000;

    map->intersections.tab->laneSet.count = MAP_config.Mapconfig->intersections.tab[0].laneSet.count;

    GenericLane *GeLane = map->intersections.tab->laneSet.tab;

    for(int i = 0;i < map->intersections.tab[0].laneSet.count ;i++) {
        GenericLane *config_lane = &MAP_config.Mapconfig->intersections.tab[0].laneSet.tab[i];
        
        GeLane[i].laneID = config_lane->laneID;
        
        if (config_lane->egressApproach_option) {
            GeLane[i].egressApproach_option = TRUE;
            GeLane[i].egressApproach = config_lane->egressApproach;
            asn1_bstr_set_bit(&GeLane[i].laneAttributes.directionalUse, LaneDirection_egressPath);
        }
        if (config_lane->ingressApproach_option) {
            GeLane[i].ingressApproach_option = TRUE;
            GeLane[i].ingressApproach = config_lane->ingressApproach;
            asn1_bstr_set_bit(&GeLane[i].laneAttributes.directionalUse, LaneDirection_ingressPath);
        }

        GeLane[i].laneAttributes.laneType.choice = LaneTypeAttributes_vehicle;

        GeLane[i].nodeList.choice = NodeListXY_nodes;
        GeLane[i].nodeList.u.nodes.count = config_lane->nodeList.u.nodes.count;

        Malloc(GeLane[i].nodeList.u.nodes.tab, sizeof(NodeXY) * GeLane[i].nodeList.u.nodes.count, "MAP_init_NodeXY_new");

        for(int j = 0 ; j < GeLane[i].nodeList.u.nodes.count; j++) {
            GeLane[i].nodeList.u.nodes.tab[j].delta.choice = NodeOffsetPointXY_node_LatLon;
            GeLane[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lat = config_lane->nodeList.u.nodes.tab[j].delta.u.node_LatLon.lat;
            GeLane[i].nodeList.u.nodes.tab[j].delta.u.node_LatLon.lon = config_lane->nodeList.u.nodes.tab[j].delta.u.node_LatLon.lon;
        }
    }
    return ;
}


void map_signal_group(MapData *map, int SubPhaseCount_index, int SignalCount_index) {
    GenericLane *GeLane = map->intersections.tab->laneSet.tab;
    int j = SignalCount_index;
    uint8_t SignalStatus = get_SignalStatus(SubPhaseCount_index - 1, SignalCount_index);
    
    // 去 and SignalStatus_t
    if(SignalStatus & GREEN) {
        for(int i = 0; i < MAP_config.map_lane2connecting.Direction[j].Lane_count; i++) {
            int LANEID = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LaneID;
            for(int k = 0; k < map->intersections.tab[0].laneSet.count; k++) {
                if(GeLane[k].laneID == LANEID) {
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LeftconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].StrightconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].RightconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                }
            }
        }
    }
    if(SignalStatus & LEFT_GREEN) {
        for(int i = 0;i < MAP_config.map_lane2connecting.Direction[j].Lane_count ;i++) {
            int LANEID = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LaneID;
            for(int k = 0;k < map->intersections.tab[0].laneSet.count ;k++) {
                if(GeLane[k].laneID == LANEID) {
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LeftconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                }
            }
        }
    }
    if(SignalStatus & STRAIGHT_GREEN) {
        for(int i = 0;i < MAP_config.map_lane2connecting.Direction[j].Lane_count ;i++) {
            int LANEID = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LaneID;
            for(int k = 0;k < map->intersections.tab[0].laneSet.count ;k++) {
                if(GeLane[k].laneID == LANEID) {
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].StrightconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                }
            }
        }
    }
    if(SignalStatus & RIGHT_GREEN) {
        for(int i = 0;i < MAP_config.map_lane2connecting.Direction[j].Lane_count ;i++) {
            int LANEID = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].LaneID;
            for(int k = 0;k < map->intersections.tab[0].laneSet.count ;k++) {
                if(GeLane[k].laneID == LANEID) {
                    for(int connect_lane = 0; connect_lane < LANE_MAX_NUMBER; connect_lane++) {
                        int con_lane_id = MAP_config.map_lane2connecting.Direction[j].connectingLane[i].RightconnectingLane[connect_lane];
                        if(!con_lane_id)
                            continue;
                        for(int f_connect_lane = 0; f_connect_lane < GeLane[k].connectsTo.count; f_connect_lane++) {
                            if(GeLane[k].connectsTo.tab[f_connect_lane].connectingLane.lane == con_lane_id) {
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup_option = TRUE;
                                GeLane[k].connectsTo.tab[f_connect_lane].signalGroup = SubPhaseCount_index;
                            }
                        }
                    }
                }
            }
        }
    }
}

void map_msg_update(MapData *map)
{
    uint8_t SubPhaseCount = get_SubPhaseCount();
    uint8_t SignalCount = get_SignalCount();
    uint8_t current_phase = get_current_phase();
    
    map->intersections.tab->revision++;
    map->intersections.tab->revision &= 0b1111111;
    
    for(int i = SubPhaseCount;i > 0;i--) {
        for(int j = 0;j < SignalCount;j++){
            map_signal_group(map, i, j);
        }
    }
    for(int j = 0;j < SignalCount;j++){
        map_signal_group(map, current_phase, j);
    }
}

void map_dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *)data;
    for (count = 0; count < len; count++) {
        if (count % 16 == 0)
            printf("\n");

        printf("%02X ", p[count]);
    }
    printf("\n\n");
}

void map_print(MapData *map)
{
    int intersection_index, speedLimits_index, lane_index, node_index, connectionTo_index, maneuver_index;
    printf("Decoded MAP\n");
    if (map->intersections_option) {
        printf("intersections count: %d\n", map->intersections.count);
        for (intersection_index = 0; intersection_index < map->intersections.count; intersection_index++) {
            printf("intersection [%d]:", intersection_index);
            if (map->intersections.tab[intersection_index].id.region_option) {
                printf("region %d,", map->intersections.tab[intersection_index].id.region);
            }
            printf(" id: %d, revision: %d\n", map->intersections.tab[intersection_index].id.id, map->intersections.tab[intersection_index].revision);
            printf(" refPoint latitude: %d, longitude: %d\n", map->intersections.tab[intersection_index].refPoint.lat, map->intersections.tab[intersection_index].refPoint.Long);
            if (map->intersections.tab[intersection_index].laneWidth_option) {
                printf(" laneWidth: %d\n", map->intersections.tab[intersection_index].laneWidth);
            }
            if (map->intersections.tab[intersection_index].speedLimits_option) {
                for (speedLimits_index = 0; speedLimits_index < map->intersections.tab[intersection_index].speedLimits.count; speedLimits_index++) {
                    printf(" speedLimit[%d] type: %d, speed value: %d\n", speedLimits_index, map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].type, map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].speed);
                }
            }
            printf(" Lane set count : %d\n", map->intersections.tab[intersection_index].laneSet.count);
            for (lane_index = 0; lane_index < map->intersections.tab[intersection_index].laneSet.count; lane_index++) {
                printf(" Lane[%d] ID: %d, lane type: %d\n", lane_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneID, map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneAttributes.laneType.choice);
                /* Mapping union format based on choice */
                if (NodeListXY_nodes == map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice) {
                    /* The function only show nodes type */
                    printf("  node list count: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count);
                    printf("  node list ingressApproach: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].ingressApproach);
                    printf("  node list egressApproach: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].egressApproach);
                    for (node_index = 0; node_index < map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count; node_index++) {
                        switch (map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice) {
                            case NodeOffsetPointXY_node_XY1:
                                printf("   [%d] node_XY1 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.y);
                                break;
                            case NodeOffsetPointXY_node_XY2:
                                printf("   [%d] node_XY2 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.y);
                                break;
                            case NodeOffsetPointXY_node_XY3:
                                printf("   [%d] node_XY3 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.y);
                                break;
                            case NodeOffsetPointXY_node_XY4:
                                printf("   [%d] node_XY4 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.y);
                                break;
                            case NodeOffsetPointXY_node_XY5:
                                printf("   [%d] node_XY5 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.y);
                                break;
                            case NodeOffsetPointXY_node_XY6:
                                printf("   [%d] node_XY6 x: %d, y: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.y);
                                break;
                            case NodeOffsetPointXY_node_LatLon:
                                printf("   [%d] LatLon latitude: %d, longitude: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lat, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lon);
                                break;
                            default:
                                printf("   [%d] Unhandled delta choice type: %d\n", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice);
                                break;
                        }
                    }
                }
                else {
                    printf("  Unhandled node choice type: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice);
                }
                if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo_option) {
                    printf("  ConnectsToList count: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count);
                    for (connectionTo_index = 0; connectionTo_index < map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count; connectionTo_index++) {
                        printf("  ConnectsToList[%d]\n", connectionTo_index);
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].remoteIntersection_option) {
                            if (map->intersections.tab[intersection_index].id.region_option) {
                                printf("   remoteIntersection region: %d,", map->intersections.tab[intersection_index].id.region);
                            }
                            printf(" id: %d, revision: %d\n", map->intersections.tab[intersection_index].id.id, map->intersections.tab[intersection_index].revision);
                        }
                        printf("   connectingLane lane: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.lane);
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver_option) {
                            printf("   connectingLane maneuver:");
                            for (maneuver_index = 0; maneuver_index < AllowedManeuvers_MAX_BITS; maneuver_index++) {
                                if (asn1_bstr_is_bit_set(&(map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver), maneuver_index)) {
                                    switch (maneuver_index) {
                                        case AllowedManeuvers_maneuverStraightAllowed:
                                            printf(" maneuverStraightAllowed");
                                            break;
                                        case AllowedManeuvers_maneuverLeftAllowed:
                                            printf(" maneuverLeftAllowed");
                                            break;
                                        case AllowedManeuvers_maneuverRightAllowed:
                                            printf(" maneuverRightAllowed");
                                            break;
                                        default:
                                            printf(" bit %d", maneuver_index);
                                            break;
                                    }
                                }
                            }
                            printf("\n");
                        }
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup_option) {
                            printf("   signalGroup: %d\n", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup);
                        }
                    }
                }
            }
        }
    }
    return;
}

void map_decode(uint8_t *rx_buf, int rx_buf_len)
{
    int ret;
    /* a pointer to containing decoded msg */
    MessageFrame *p_msgf;

    printf("MAP decoding data:\n");
    map_dump_mem(rx_buf, rx_buf_len);

    ret = j2735_msg_decode(&p_msgf, rx_buf, rx_buf_len, NULL);
    if (ret < 0) {
        /* handling the decoding error */
        printf("decode msg error\n");
    }
    else if ((ret > 0) && (p_msgf->messageId == MapData_Id)) {
        map_print((MapData *)(p_msgf->u.data));
        J2735_FREE_MSG_FRAME(p_msgf);
    }

    return;
}

