
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TIB_MAP_utils.h"
#include "TIB_config.h"
#include "TIB_utils.h"
#include "config.h"
#include "error_code_user.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "traffic_signal_status_updating.h"

LOG_USE_MODULE(TIB);

MapData *map;

void map_msg_init(MapData **map_ptr)
{
    Malloc(*map_ptr, sizeof(MapData), "MapData");
    MapData *map = *map_ptr;

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
        lane->laneAttributes.sharedWith.len = 10;
        Malloc(lane->laneAttributes.sharedWith.buf, 2, "sharedWith");

        // bit 5-8 為 Approach
        lane->laneID = ((config_lane->approach << 5) & 0b11100000);
        // ingress 設 bit 4 為 1, egress 為 0
        if (config_lane->direction & (1 << LaneDirection_ingressPath)) {
            lane->laneID |= 0b00010000;
        }
        lane->laneID |= (0b00001111 & config_lane->lane_index);

        lane->laneAttributes.directionalUse.len = 2;
        Malloc(lane->laneAttributes.directionalUse.buf, 1, "directionalUse");
        if (config_lane->direction & (1 << LaneDirection_egressPath)) {
            lane->egressApproach_option = TRUE;
            lane->egressApproach = config_lane->approach;
            asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_egressPath);
        }
        if (config_lane->direction & (1 << LaneDirection_ingressPath)) {
            lane->ingressApproach_option = TRUE;
            lane->ingressApproach = config_lane->approach;
            asn1_bstr_set_bit(&lane->laneAttributes.directionalUse, LaneDirection_ingressPath);
        }

        lane->laneAttributes.laneType.choice = config_lane->lane_type;
        lane->laneAttributes.laneType.u.vehicle.len = 16;
        Malloc(lane->laneAttributes.laneType.u.vehicle.buf, 2, "laneType");
        for (uint16_t mask = 1, i = 0; mask != 0; mask <<= 1, i++) {
            if (mask & config_lane->lane_attributes) {
                asn1_bstr_set_bit(&lane->laneAttributes.laneType.u.vehicle, i);
            }
        }

        lane->nodeList.choice = NodeListXY_nodes;
        lane->nodeList.u.nodes.count =
            config_lane->node_list.size > NodeSetXY_MAX_SIZE ? NodeSetXY_MAX_SIZE : config_lane->node_list.size;

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

static inline int map_connectTo_clean(MapData *map)
{
    if (map->intersections.count <= 0)
        return -1;
    LaneList *laneList = &map->intersections.tab[0].laneSet;
    for (int i = 0; i < laneList->count; i++) {
        laneList->tab[i].connectsTo_option = FALSE;
        laneList->tab[i].connectsTo.count = 0;
    }
    return 1;
}

int map_signal_group(MapData *map)
{
    map_connectTo_clean(map);
    LaneList *laneSet = &map->intersections.tab->laneSet;

#define set_compass_connectsTo(compass)                                         \
    do {                                                                        \
        for (int k = 0; k < config_connectTo->compass.size; k++) {              \
            uint8_t connectTo_laneId = vector_at(config_connectTo->compass, k); \
            lane->connectsTo_option = TRUE;                                     \
            lane->connectsTo.tab[connectsTo_count].connectingLane.lane =        \
                laneSet->tab[connectTo_laneId].laneID;                          \
            lane->connectsTo.tab[connectsTo_count].signalGroup_option = TRUE;   \
            lane->connectsTo.tab[connectsTo_count].signalGroup = signalGroupID; \
            lane->connectsTo.count = ++connectsTo_count;                        \
        }                                                                       \
    } while (0)

#define search_signal_compass(signalMask, setFunc)                                   \
    do {                                                                             \
        if (signalGroupID != -1) {                                                   \
            for (int i = 0; i < TIB_config.connectsTo_list.size; i++) {              \
                MAP_config_connectsTo_t *config_connectTo =                          \
                    &vector_at(TIB_config.connectsTo_list, i);                       \
                int connectsTo_count = lane->connectsTo.count;                       \
                if (config_connectTo->config_laneID == config_lane->config_laneID) { \
                    setFunc                                                          \
                }                                                                    \
            }                                                                        \
        }                                                                            \
    } while (0)

    int signalGroupID = -1;

    for (int i = 0; i < COMPASS_NUM; i++) {
        struct list_head *head = &TIB_config.MAP_lane_approach[i];
        MAP_config_lane_t *config_lane, *safe;
        // 一般車道
        list_for_each_entry_safe(config_lane, safe, head, approach_node)
        {
            GenericLane *lane = &laneSet->tab[config_lane->config_laneID];
            signalGroupID = TIB_config.signalGroupId_table[config_lane->approach][RroundHeadGreenIndex];
            search_signal_compass(RroundHeadGreenMask, set_compass_connectsTo(left_laneId);
                                  set_compass_connectsTo(straight_laneId);
                                  set_compass_connectsTo(right_laneId););
            signalGroupID = TIB_config.signalGroupId_table[config_lane->approach][LeftGreenIndex];
            search_signal_compass(LeftGreenMask, set_compass_connectsTo(left_laneId););
            signalGroupID = TIB_config.signalGroupId_table[config_lane->approach][StraightGreenIndex];
            search_signal_compass(StraightGreenMask, set_compass_connectsTo(straight_laneId););
            signalGroupID = TIB_config.signalGroupId_table[config_lane->approach][RightGreenIndex];
            search_signal_compass(RightGreenMask, set_compass_connectsTo(right_laneId););
        }
        head = &TIB_config.MAP_sidewalk_approach[i];
        // 行人道
        list_for_each_entry_safe(config_lane, safe, head, approach_node)
        {
            GenericLane *lane = &laneSet->tab[config_lane->config_laneID];
            signalGroupID = TIB_config.signalGroupId_table[i][PedestrianGreenIndex];
            if (signalGroupID)
                search_signal_compass(LeftGreenMask, set_compass_connectsTo(straight_laneId););
        }
    }
#undef set_compass_connectsTo
#undef search_signal_compass
    return 1;
}

int map_msg_update(MapData *map)
{
    map->intersections.tab->revision++;
    map->intersections.tab->revision &= 0b1111111;
    return map_signal_group(map);
}

void map_dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *) data;
    char data_str[LOG_CONTENT_LEN + 1];
    memset(data_str, 0, sizeof(data_str));
    for (count = 0; count < len; count++) {
        if (count % 16 == 0) {
            LOG_MSG_APPEND(data_str, "\n");
        }

        LOG_MSG_APPEND(data_str, "%02X ", p[count]);
    }
    LOG_MSG_APPEND(data_str, "\n");
    LOG_MSG_TRACE(data_str);
}

void map_print(MapData *map)
{
    int intersection_index, speedLimits_index, lane_index, node_index, connectionTo_index, maneuver_index;
    LOG_MSG_TRACE("Decoded MAP");
    if (map->intersections_option) {
        LOG_MSG_TRACE("intersections count: %d", map->intersections.count);
        for (intersection_index = 0; intersection_index < map->intersections.count; intersection_index++) {
            char intersection_str[LOG_CONTENT_LEN + 1];
            memset(intersection_str, 0, sizeof(intersection_str));
            LOG_MSG_APPEND(intersection_str, "intersection [%d]:", intersection_index);
            if (map->intersections.tab[intersection_index].id.region_option) {
                LOG_MSG_APPEND(intersection_str, "region %d,", map->intersections.tab[intersection_index].id.region);
            }
            LOG_MSG_APPEND(intersection_str, " id: %d, revision: %d\n", map->intersections.tab[intersection_index].id.id, map->intersections.tab[intersection_index].revision);
            LOG_MSG_APPEND(intersection_str, " refPoint latitude: %d, longitude: %d\n", map->intersections.tab[intersection_index].refPoint.lat, map->intersections.tab[intersection_index].refPoint.Long);
            LOG_MSG_TRACE(intersection_str);

            if (map->intersections.tab[intersection_index].laneWidth_option) {
                LOG_MSG_TRACE(intersection_str, " laneWidth: %d", map->intersections.tab[intersection_index].laneWidth);
            }
            if (map->intersections.tab[intersection_index].speedLimits_option) {
                for (speedLimits_index = 0; speedLimits_index < map->intersections.tab[intersection_index].speedLimits.count; speedLimits_index++) {
                    LOG_MSG_TRACE(intersection_str, " speedLimit[%d] type: %d, speed value: %d", speedLimits_index, map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].type, map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].speed);
                }
            }
            LOG_MSG_TRACE(" Lane set count : %d", map->intersections.tab[intersection_index].laneSet.count);
            for (lane_index = 0; lane_index < map->intersections.tab[intersection_index].laneSet.count; lane_index++) {
                LOG_MSG_TRACE(" Lane[%d] ID: %d, lane type: %d", lane_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneID, map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneAttributes.laneType.choice);
                /* Mapping union format based on choice */
                if (NodeListXY_nodes == map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice) {
                    /* The function only show nodes type */
                    LOG_MSG_TRACE("  node list count: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count);
                    if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].ingressApproach_option)
                        LOG_MSG_TRACE("  node list ingressApproach: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].ingressApproach);
                    if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].egressApproach_option)
                        LOG_MSG_TRACE("  node list egressApproach: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].egressApproach);
                    for (node_index = 0; node_index < map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count; node_index++) {
                        switch (map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice) {
                        case NodeOffsetPointXY_node_XY1:
                            LOG_MSG_TRACE("   [%d] node_XY1 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.y);
                            break;
                        case NodeOffsetPointXY_node_XY2:
                            LOG_MSG_TRACE("   [%d] node_XY2 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.y);
                            break;
                        case NodeOffsetPointXY_node_XY3:
                            LOG_MSG_TRACE("   [%d] node_XY3 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.y);
                            break;
                        case NodeOffsetPointXY_node_XY4:
                            LOG_MSG_TRACE("   [%d] node_XY4 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.y);
                            break;
                        case NodeOffsetPointXY_node_XY5:
                            LOG_MSG_TRACE("   [%d] node_XY5 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.y);
                            break;
                        case NodeOffsetPointXY_node_XY6:
                            LOG_MSG_TRACE("   [%d] node_XY6 x: %d, y: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.x, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.y);
                            break;
                        case NodeOffsetPointXY_node_LatLon:
                            LOG_MSG_TRACE("   [%d] LatLon latitude: %d, longitude: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lat, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lon);
                            break;
                        default:
                            LOG_MSG_TRACE("   [%d] Unhandled delta choice type: %d", node_index, map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice);
                            break;
                        }
                    }
                } else {
                    LOG_MSG_TRACE("  Unhandled node choice type: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice);
                }
                if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo_option) {
                    LOG_MSG_TRACE("  ConnectsToList count: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count);
                    for (connectionTo_index = 0; connectionTo_index < map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count; connectionTo_index++) {
                        LOG_MSG_TRACE("  ConnectsToList[%d]", connectionTo_index);
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].remoteIntersection_option) {
                            if (map->intersections.tab[intersection_index].id.region_option) {
                                LOG_MSG_TRACE("   remoteIntersection region: %d,", map->intersections.tab[intersection_index].id.region);
                            }
                            LOG_MSG_TRACE(" id: %d, revision: %d", map->intersections.tab[intersection_index].id.id, map->intersections.tab[intersection_index].revision);
                        }
                        LOG_MSG_TRACE("   connectingLane lane: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.lane);
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver_option) {
                            char maneuver_str[LOG_CONTENT_LEN + 1];
                            memset(maneuver_str, 0, sizeof(maneuver_str));
                            LOG_MSG_APPEND(maneuver_str, "   connectingLane maneuver:");
                            for (maneuver_index = 0; maneuver_index < AllowedManeuvers_MAX_BITS; maneuver_index++) {
                                if (asn1_bstr_is_bit_set(&(map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver), maneuver_index)) {
                                    switch (maneuver_index) {
                                    case AllowedManeuvers_maneuverStraightAllowed:
                                        LOG_MSG_APPEND(maneuver_str, " maneuverStraightAllowed");
                                        break;
                                    case AllowedManeuvers_maneuverLeftAllowed:
                                        LOG_MSG_APPEND(maneuver_str, " maneuverLeftAllowed");
                                        break;
                                    case AllowedManeuvers_maneuverRightAllowed:
                                        LOG_MSG_APPEND(maneuver_str, " maneuverRightAllowed");
                                        break;
                                    default:
                                        LOG_MSG_APPEND(maneuver_str, " bit %d", maneuver_index);
                                        break;
                                    }
                                }
                                LOG_MSG_TRACE(maneuver_str);
                            }
                        }
                        if (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup_option) {
                            LOG_MSG_TRACE("   signalGroup: %d", map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup);
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

    LOG_MSG_TRACE("MAP decoding data:");
    map_dump_mem(rx_buf, rx_buf_len);

    ret = j2735_msg_decode(&p_msgf, rx_buf, rx_buf_len, NULL);
    if (ret < 0) {
        /* handling the decoding error */
        LOG_MSG_TRACE("decode msg error");
    } else if ((ret > 0) && (p_msgf->messageId == MapData_Id)) {
        map_print((MapData *) (p_msgf->u.data));
        J2735_FREE_MSG_FRAME(p_msgf);
    }

    return;
}
