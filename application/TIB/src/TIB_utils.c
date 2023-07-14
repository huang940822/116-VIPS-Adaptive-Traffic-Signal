
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TIB_config.h"
#include "TIB_utils.h"
#include "config.h"
#include "error_code_user.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

MapData *map;

void map_msg_init(MapData *map)
{
    map->timeStamp_option = FALSE;
    map->msgIssueRevision = 0;

    map->intersections_option = TRUE;
    map->intersections.count = 1;

    Malloc(map->intersections.tab, sizeof(IntersectionGeometry), "IntersectionGeometry");
    IntersectionGeometry *intersection = map->intersections.tab;
    intersection->id.id = config.RSU_id;
    intersection->id.region_option = TRUE;
    intersection->id.region = config.RSU_region;

    intersection->revision = 0;

    intersection->refPoint.lat = config.RSU_lat * 10000000;
    intersection->refPoint.Long = config.RSU_lon * 10000000;
    intersection->refPoint.elevation_option = TRUE;
    intersection->refPoint.elevation = config.RSU_elev * 10000000;

    intersection->laneSet.count = TIB_config.lane_list.size;
    Malloc(intersection->laneSet.tab, sizeof(GenericLane) * intersection->laneSet.count, "GenericLane");

    for (int i = 0; i < intersection->laneSet.count; i++) {
        GenericLane *lane = &intersection->laneSet.tab[i];
        MAP_config_lane_t *config_lane = &vector_at(TIB_config.lane_list, i);

        // bit 5-8 為 Approach
        lane->laneID = ((config_lane->approach << 5) & 0b11100000);
        // ingress 設 bit 4 為 0, egress 為 0
        if (config_lane->direction == LaneDirection_ingressPath) {
            lane->laneID |= 0b00010000;
        }
        lane->laneID |= (0b00001111 & config_lane->lane_index);

        if (config_lane->direction == LaneDirection_ingressPath) {
            lane->egressApproach_option = TRUE;
            lane->egressApproach = LaneDirection_ingressPath;
            asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_ingressPath);
        } else {
            lane->ingressApproach_option = TRUE;
            lane->ingressApproach = LaneDirection_egressPath;
            asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_egressPath);
        }

        lane->laneAttributes.laneType.choice = LaneTypeAttributes_vehicle;

        lane->nodeList.choice = NodeListXY_nodes;
        lane->nodeList.u.nodes.count = config_lane->node_list.size;

        Malloc(lane->nodeList.u.nodes.tab, sizeof(NodeXY) * lane->nodeList.u.nodes.count, "MAP_init_NodeXY_new");
        Malloc(lane->connectsTo.tab, sizeof(Connection) * ConnectsToList_MAX_SIZE, "Connection");

        for (int j = 0; j < config_lane->node_list.size; j++) {
            MAP_Node_t *node = &vector_at(config_lane->node_list, j);
            lane->nodeList.u.nodes.tab[j].delta.choice = NodeOffsetPointXY_node_LatLon;
            lane->nodeList.u.nodes.tab[j].delta.u.node_LatLon.lat = node->lat * 10000000;
            lane->nodeList.u.nodes.tab[j].delta.u.node_LatLon.lon = node->lon * 10000000;
        }
    }
}

void map_connectTo_clean(MapData *map)
{
    if (map->intersections_option == FALSE && map->intersections.count <= 0)
        return;
    LaneList *laneList = &map->intersections.tab[0].laneSet;
    for (int i = 0; i < laneList->count; i++) {
        if (laneList->tab[i].connectsTo_option == FALSE)
            continue;
        laneList->tab[i].connectsTo_option = FALSE;
        laneList->tab[i].connectsTo.count = 0;
    }
}

#define RroundHeadGreen 0b00100000
#define LeftGreen 0b00010000
#define StrightGreen 0b00001000
#define RightGreen 0b00000100
const uint8_t GreenMasks[] = {RroundHeadGreen, LeftGreen, StrightGreen, RightGreen};

void map_signal_group(MapData *map)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    map_connectTo_clean(map);
    GenericLane *lane = map->intersections.tab->laneSet.tab;

    int signalGroupID = 1;

    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for (int j = 0; j < signal_status.SignalCount; j++) {
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

    map_signal_group(map);
}

void map_dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *) data;
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
                } else {
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
    } else if ((ret > 0) && (p_msgf->messageId == MapData_Id)) {
        map_print((MapData *) (p_msgf->u.data));
        J2735_FREE_MSG_FRAME(p_msgf);
    }

    return;
}
