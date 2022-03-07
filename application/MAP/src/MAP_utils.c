#include "MAP_utils.h"
#include <stdio.h>
#include "error_code_user.h"
#include "j2735_codec.h"
#include "j2735_msg.h"
void compose_map(uint8_t **tx_buf, int *tx_buf_len)
{
    MessageFrame msgf;
    MapData *map;
    J2735CodecErr j2735_err;

    /* Make sure we reset the data structure at least once. */
    memset(&msgf, 0, sizeof(msgf));
    /* all fields which should be allocated before using are allocated
     * recursively */
    map = (MapData *) j2735_msg_prealloc(MapData_Id);

    map->timeStamp_option = FALSE;
    map->msgIssueRevision = 0;
    map->layerType_option = TRUE;
    map->layerType = LayerType_intersectionData;
    map->layerID_option = TRUE;
    map->layerID = 1;
    map->intersections_option = TRUE;

    map->intersections.count = 1;

    map->intersections.tab[0].name_option = FALSE;
    map->intersections.tab[0].id.region_option = FALSE;
    map->intersections.tab[0].id.id = 4009;
    map->intersections.tab[0].revision = 102;
    map->intersections.tab[0].refPoint.lat = 248087872;
    map->intersections.tab[0].refPoint.Long = 1210358460;
    map->intersections.tab[0].refPoint.elevation_option = TRUE;
    map->intersections.tab[0].refPoint.elevation = 640;
    map->intersections.tab[0].refPoint.regional_option = FALSE;
    map->intersections.tab[0].laneWidth_option = TRUE;
    map->intersections.tab[0].laneWidth = 360;
    map->intersections.tab[0].speedLimits_option = TRUE;

    map->intersections.tab[0].speedLimits.count = 1;
    map->intersections.tab[0].speedLimits.tab[0].type =
        SpeedLimitType_vehicleMaxSpeed;
    map->intersections.tab[0].speedLimits.tab[0].speed = 693;

    map->intersections.tab[0].laneSet.count = 12;
    /* Lane 1 */
    map->intersections.tab[0].laneSet.tab[0].laneID = 1;
    map->intersections.tab[0].laneSet.tab[0].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[0].ingressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[0].ingressApproach = 1;
    map->intersections.tab[0].laneSet.tab[0].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[0]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[0]
                            .laneAttributes.directionalUse),
                      LaneDirection_ingressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[0].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[0].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[0]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[0].laneAttributes.regional_option =
        FALSE;

    map->intersections.tab[0].laneSet.tab[0].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[0].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[0].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[0].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 298;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 2090;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 814;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 1537;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = 759;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = 1429;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[0].connectsTo_option = TRUE;
    map->intersections.tab[0].laneSet.tab[0].connectsTo.count = 3;

    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .connectingLane.lane = 4;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[0]
                          .connectsTo.tab[0]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[0]
                            .connectsTo.tab[0]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverStraightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[0].connectsTo.tab[0].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[0]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .connectingLane.lane = 2;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[0]
                          .connectsTo.tab[1]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[0]
                            .connectsTo.tab[1]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverRightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[0].connectsTo.tab[1].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[1]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .connectingLane.lane = 6;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[0]
                          .connectsTo.tab[2]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[0]
                            .connectsTo.tab[2]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverLeftAllowed);
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[0].connectsTo.tab[2].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[0]
        .connectsTo.tab[2]
        .connectionID_option = FALSE;

    map->intersections.tab[0].laneSet.tab[0].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[0].regional_option = FALSE;
    /* Lane 2 */
    map->intersections.tab[0].laneSet.tab[1].laneID = 2;
    map->intersections.tab[0].laneSet.tab[1].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[1].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[1].egressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[1].egressApproach = 2;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[1]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[1]
                            .laneAttributes.directionalUse),
                      LaneDirection_egressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[1].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[1].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[1]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[1].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[1].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[1].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[1].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[1].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -1817;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 1025;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -1492;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 890;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = -1383;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = 755;
    map->intersections.tab[0]
        .laneSet.tab[1]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[1].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[1].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[1].regional_option = FALSE;
    /* Lane 3 */
    map->intersections.tab[0].laneSet.tab[2].laneID = 3;
    map->intersections.tab[0].laneSet.tab[2].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[2].ingressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[2].ingressApproach = 3;
    map->intersections.tab[0].laneSet.tab[2].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[2]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[2]
                            .laneAttributes.directionalUse),
                      LaneDirection_ingressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[2].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[2].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[2]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[2].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[2].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[2].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[2].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[2].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -2142;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 310;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -1763;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 998;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = -1329;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = 755;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[2].connectsTo_option = TRUE;
    map->intersections.tab[0].laneSet.tab[2].connectsTo.count = 3;

    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .connectingLane.lane = 6;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[2]
                          .connectsTo.tab[0]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[2]
                            .connectsTo.tab[0]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverStraightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[2].connectsTo.tab[0].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[0]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .connectingLane.lane = 4;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[2]
                          .connectsTo.tab[1]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[2]
                            .connectsTo.tab[1]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverRightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[2].connectsTo.tab[1].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[1]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .connectingLane.lane = 8;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[2]
                          .connectsTo.tab[2]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[2]
                            .connectsTo.tab[2]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverLeftAllowed);
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[2].connectsTo.tab[2].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[2]
        .connectsTo.tab[2]
        .connectionID_option = FALSE;

    map->intersections.tab[0].laneSet.tab[2].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[2].regional_option = FALSE;
    /* Lane 4 */
    map->intersections.tab[0].laneSet.tab[3].laneID = 4;
    map->intersections.tab[0].laneSet.tab[3].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[3].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[3].egressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[3].egressApproach = 4;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[3]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[3]
                            .laneAttributes.directionalUse),
                      LaneDirection_egressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[3].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[3].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[3]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[3].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[3].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[3].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[3].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[3].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -1464;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -1416;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -868;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -1780;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = -732;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = -1564;
    map->intersections.tab[0]
        .laneSet.tab[3]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[3].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[3].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[3].regional_option = FALSE;
    /* Lane 5 */
    map->intersections.tab[0].laneSet.tab[4].laneID = 5;
    map->intersections.tab[0].laneSet.tab[4].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[4].ingressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[4].ingressApproach = 5;
    map->intersections.tab[0].laneSet.tab[4].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[4]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[4]
                            .laneAttributes.directionalUse),
                      LaneDirection_ingressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[4].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[4].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[4]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[4].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[4].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[4].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[4].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[4].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -190;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -1982;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -868;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -1780;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = -732;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = -1591;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[4].connectsTo_option = TRUE;
    map->intersections.tab[0].laneSet.tab[4].connectsTo.count = 3;

    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .connectingLane.lane = 8;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[4]
                          .connectsTo.tab[0]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[4]
                            .connectsTo.tab[0]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverStraightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[4].connectsTo.tab[0].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[0]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .connectingLane.lane = 6;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[4]
                          .connectsTo.tab[1]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[4]
                            .connectsTo.tab[1]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverRightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[4].connectsTo.tab[1].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[1]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .connectingLane.lane = 2;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[4]
                          .connectsTo.tab[2]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[4]
                            .connectsTo.tab[2]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverLeftAllowed);
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[4].connectsTo.tab[2].signalGroup = 1;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[4]
        .connectsTo.tab[2]
        .connectionID_option = FALSE;

    map->intersections.tab[0].laneSet.tab[4].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[4].regional_option = FALSE;
    /* Lane 6 */
    map->intersections.tab[0].laneSet.tab[5].laneID = 6;
    map->intersections.tab[0].laneSet.tab[5].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[5].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[5].egressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[5].egressApproach = 6;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[5]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[5]
                            .laneAttributes.directionalUse),
                      LaneDirection_egressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[5].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[5].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[5]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[5].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[5].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[5].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[5].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[5].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 1546;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -1551;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 1437;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -674;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = 1627;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = -701;
    map->intersections.tab[0]
        .laneSet.tab[5]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[5].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[5].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[5].regional_option = FALSE;
    /* Lane 7 */
    map->intersections.tab[0].laneSet.tab[6].laneID = 7;
    map->intersections.tab[0].laneSet.tab[6].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[6].ingressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[6].ingressApproach = 7;
    map->intersections.tab[0].laneSet.tab[6].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[6]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[6]
                            .laneAttributes.directionalUse),
                      LaneDirection_ingressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[6].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[6].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[6]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[6].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[6].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[6].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[6].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[6].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 1980;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -782;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 1464;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -674;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = 1573;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = -782;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[6].connectsTo_option = TRUE;
    map->intersections.tab[0].laneSet.tab[6].connectsTo.count = 3;

    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .connectingLane.lane = 2;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[6]
                          .connectsTo.tab[0]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[6]
                            .connectsTo.tab[0]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverStraightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[6].connectsTo.tab[0].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[0]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .connectingLane.lane = 8;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[6]
                          .connectsTo.tab[1]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[6]
                            .connectsTo.tab[1]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverRightAllowed);
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[6].connectsTo.tab[1].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[1]
        .connectionID_option = FALSE;

    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .connectingLane.lane = 4;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .connectingLane.maneuver_option = TRUE;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[6]
                          .connectsTo.tab[2]
                          .connectingLane.maneuver),
                    AllowedManeuvers_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[6]
                            .connectsTo.tab[2]
                            .connectingLane.maneuver),
                      AllowedManeuvers_maneuverLeftAllowed);
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .remoteIntersection_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .signalGroup_option = TRUE;
    map->intersections.tab[0].laneSet.tab[6].connectsTo.tab[2].signalGroup = 2;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .userClass_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[6]
        .connectsTo.tab[2]
        .connectionID_option = FALSE;

    map->intersections.tab[0].laneSet.tab[6].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[6].regional_option = FALSE;
    /* Lane 8 */
    map->intersections.tab[0].laneSet.tab[7].laneID = 8;
    map->intersections.tab[0].laneSet.tab[7].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[7].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[7].egressApproach_option = TRUE;
    map->intersections.tab[0].laneSet.tab[7].egressApproach = 8;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[7]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_set_bit(&(map->intersections.tab[0]
                            .laneSet.tab[7]
                            .laneAttributes.directionalUse),
                      LaneDirection_egressPath);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[7].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[7].laneAttributes.laneType.choice =
        LaneTypeAttributes_vehicle;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[7]
                          .laneAttributes.laneType.u.vehicle),
                    LaneAttributes_Vehicle_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[7].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[7].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[7].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[7].nodeList.u.nodes.count = 3;
    map->intersections.tab[0].laneSet.tab[7].nodeList.u.nodes.tab =
        (NodeXY *) calloc(3, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 1465;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 1470;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 868;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 1834;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[2]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.x = 705;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[2]
        .delta.u.node_XY6.y = 1429;
    map->intersections.tab[0]
        .laneSet.tab[7]
        .nodeList.u.nodes.tab[2]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[7].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[7].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[7].regional_option = FALSE;
    /* Lane 9 */
    map->intersections.tab[0].laneSet.tab[8].laneID = 9;
    map->intersections.tab[0].laneSet.tab[8].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[8].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[8].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[8]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[8].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[8].laneAttributes.laneType.choice =
        LaneTypeAttributes_crosswalk;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[8]
                          .laneAttributes.laneType.u.crosswalk),
                    LaneAttributes_Crosswalk_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[8].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[8].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[8].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[8].nodeList.u.nodes.count = 2;
    map->intersections.tab[0].laneSet.tab[8].nodeList.u.nodes.tab =
        (NodeXY *) calloc(2, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 1248;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 741;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -1492;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 782;
    map->intersections.tab[0]
        .laneSet.tab[8]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[8].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[8].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[8].regional_option = FALSE;
    /* Lane 10 */
    map->intersections.tab[0].laneSet.tab[9].laneID = 10;
    map->intersections.tab[0].laneSet.tab[9].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[9].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[9].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[9]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[9].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[9].laneAttributes.laneType.choice =
        LaneTypeAttributes_crosswalk;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[9]
                          .laneAttributes.laneType.u.crosswalk),
                    LaneAttributes_Crosswalk_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[9].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[9].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[9].nodeList.choice = NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[9].nodeList.u.nodes.count = 2;
    map->intersections.tab[0].laneSet.tab[9].nodeList.u.nodes.tab =
        (NodeXY *) calloc(2, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -1139;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = 984;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = -569;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -1079;
    map->intersections.tab[0]
        .laneSet.tab[9]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[9].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[9].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[9].regional_option = FALSE;
    /* Lane 11 */
    map->intersections.tab[0].laneSet.tab[10].laneID = 11;
    map->intersections.tab[0].laneSet.tab[10].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[10].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[10].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[10]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[10].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[10].laneAttributes.laneType.choice =
        LaneTypeAttributes_crosswalk;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[10]
                          .laneAttributes.laneType.u.crosswalk),
                    LaneAttributes_Crosswalk_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[10].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[10].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[10].nodeList.choice =
        NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[10].nodeList.u.nodes.count = 2;
    map->intersections.tab[0].laneSet.tab[10].nodeList.u.nodes.tab =
        (NodeXY *) calloc(2, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = -1302;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -553;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 1383;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = -701;
    map->intersections.tab[0]
        .laneSet.tab[10]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[10].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[10].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[10].regional_option = FALSE;
    /* Lane 12 */
    map->intersections.tab[0].laneSet.tab[11].laneID = 12;
    map->intersections.tab[0].laneSet.tab[11].name_option = FALSE;
    map->intersections.tab[0].laneSet.tab[11].ingressApproach_option = FALSE;
    map->intersections.tab[0].laneSet.tab[11].egressApproach_option = FALSE;

    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[11]
                          .laneAttributes.directionalUse),
                    LaneDirection_MAX_BITS);
    asn1_bstr_alloc(
        &(map->intersections.tab[0].laneSet.tab[11].laneAttributes.sharedWith),
        LaneSharing_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[11].laneAttributes.laneType.choice =
        LaneTypeAttributes_crosswalk;
    asn1_bstr_alloc(&(map->intersections.tab[0]
                          .laneSet.tab[11]
                          .laneAttributes.laneType.u.crosswalk),
                    LaneAttributes_Crosswalk_MAX_BITS);
    map->intersections.tab[0].laneSet.tab[11].laneAttributes.regional_option =
        FALSE;
    map->intersections.tab[0].laneSet.tab[11].maneuvers_option = FALSE;

    map->intersections.tab[0].laneSet.tab[11].nodeList.choice =
        NodeListXY_nodes;
    map->intersections.tab[0].laneSet.tab[11].nodeList.u.nodes.count = 2;
    map->intersections.tab[0].laneSet.tab[11].nodeList.u.nodes.tab =
        (NodeXY *) calloc(2, sizeof(NodeXY));
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[0]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.x = 814;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[0]
        .delta.u.node_XY6.y = -1349;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[0]
        .attributes_option = FALSE;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[1]
        .delta.choice = NodeOffsetPointXY_node_XY6;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.x = 651;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[1]
        .delta.u.node_XY6.y = 1268;
    map->intersections.tab[0]
        .laneSet.tab[11]
        .nodeList.u.nodes.tab[1]
        .attributes_option = FALSE;

    map->intersections.tab[0].laneSet.tab[11].connectsTo_option = FALSE;
    map->intersections.tab[0].laneSet.tab[11].overlays_option = FALSE;
    map->intersections.tab[0].laneSet.tab[11].regional_option = FALSE;
    /* Lane list end */
    map->intersections.tab[0].preemptPriorityData_option = FALSE;
    map->intersections.tab[0].regional_option = FALSE;

    map->roadSegments_option = FALSE;
    map->dataParameters_option = FALSE;
    map->restrictionList_option = FALSE;
    map->regional_option = FALSE;

    /* does the encoding and providing the error msg if there is */
    msgf.messageId = MapData_Id;
    msgf.u.data = map;
    // j2735_err.msg_size = ERROR_MSG;
    j2735_err.msg = error_msg;
    *tx_buf_len = j2735_msg_encode(tx_buf, &msgf, &j2735_err);
    if (*tx_buf_len <= 0) {
        printf("failed to encode the msg\n");
        printf("encode err: %s\n", j2735_err.msg);
    } else {
        printf("encode successfully\n");
    }

    /* free the memory for encoding */
    for (int i = 0; i < map->intersections.tab[0].laneSet.count; i++) {
        asn1_bstr_free(&(map->intersections.tab[0]
                             .laneSet.tab[i]
                             .laneAttributes.directionalUse));
        asn1_bstr_free(&(map->intersections.tab[0]
                             .laneSet.tab[i]
                             .laneAttributes.sharedWith));
        if (i < 8) {
            asn1_bstr_free(&(map->intersections.tab[0]
                                 .laneSet.tab[i]
                                 .laneAttributes.laneType.u.vehicle));
            if (0 == (i % 2)) {
                if (map->intersections.tab[0]
                        .laneSet.tab[i]
                        .connectsTo_option) {
                    asn1_bstr_free(&(map->intersections.tab[0]
                                         .laneSet.tab[i]
                                         .connectsTo.tab[0]
                                         .connectingLane.maneuver));
                    asn1_bstr_free(&(map->intersections.tab[0]
                                         .laneSet.tab[i]
                                         .connectsTo.tab[1]
                                         .connectingLane.maneuver));
                    asn1_bstr_free(&(map->intersections.tab[0]
                                         .laneSet.tab[i]
                                         .connectsTo.tab[2]
                                         .connectingLane.maneuver));
                }
            }
        } else {
            asn1_bstr_free(&(map->intersections.tab[0]
                                 .laneSet.tab[i]
                                 .laneAttributes.laneType.u.crosswalk));
        }
    }
    j2735_msg_dealloc(MapData_Id, map);

    return;
}

// void map_decode(uint8_t *rx_buf, int rx_buf_len)
// {
//     int ret;
//     /* a pointer to containing decoded msg */
//     MessageFrame *p_msgf;

//     printf("MAP decoding data:\n");
//     dump_mem(rx_buf, rx_buf_len);

//     ret = j2735_msg_decode(&p_msgf, rx_buf, rx_buf_len, NULL);
//     if (ret < 0) {
//         /* handling the decoding error */
//         printf("decode msg error\n");
//     }
//     else if ((ret > 0) && (p_msgf->messageId == MapData_Id)) {
//         map_print((MapData *)(p_msgf->u.data));
//         J2735_FREE_MSG_FRAME(p_msgf);
//     }

//     return;
// }

// void map_print(MapData *map)
// {
//     int intersection_index, speedLimits_index, lane_index, node_index,
//     connectionTo_index, maneuver_index; printf("Decoded MAP\n"); if
//     (map->intersections_option) {
//         printf("intersections count: %d\n", map->intersections.count);
//         for (intersection_index = 0; intersection_index <
//         map->intersections.count; intersection_index++) {
//             printf("intersection [%d]:", intersection_index);
//             if (map->intersections.tab[intersection_index].id.region_option)
//             {
//                 printf("region %d,",
//                 map->intersections.tab[intersection_index].id.region);
//             }
//             printf(" id: %d, revision: %d\n",
//             map->intersections.tab[intersection_index].id.id,
//             map->intersections.tab[intersection_index].revision); printf("
//             refPoint latitude: %d, longitude: %d\n",
//             map->intersections.tab[intersection_index].refPoint.lat,
//             map->intersections.tab[intersection_index].refPoint.Long); if
//             (map->intersections.tab[intersection_index].laneWidth_option) {
//                 printf(" laneWidth: %d\n",
//                 map->intersections.tab[intersection_index].laneWidth);
//             }
//             if
//             (map->intersections.tab[intersection_index].speedLimits_option) {
//                 for (speedLimits_index = 0; speedLimits_index <
//                 map->intersections.tab[intersection_index].speedLimits.count;
//                 speedLimits_index++) {
//                     printf(" speedLimit[%d] type: %d, speed value: %d\n",
//                     speedLimits_index,
//                     map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].type,
//                     map->intersections.tab[intersection_index].speedLimits.tab[speedLimits_index].speed);
//                 }
//             }
//             printf(" Lane set count : %d\n",
//             map->intersections.tab[intersection_index].laneSet.count); for
//             (lane_index = 0; lane_index <
//             map->intersections.tab[intersection_index].laneSet.count;
//             lane_index++) {
//                 printf(" Lane[%d] ID: %d, lane type: %d\n", lane_index,
//                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneID,
//                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].laneAttributes.laneType.choice);
//                 /* Mapping union format based on choice */
//                 if (NodeListXY_nodes ==
//                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice)
//                 {
//                     /* The function only show nodes type */
//                     printf("  node list count: %d\n",
//                     map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count);
//                     for (node_index = 0; node_index <
//                     map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.count;
//                     node_index++) {
//                         switch
//                         (map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice)
//                         {
//                             case NodeOffsetPointXY_node_XY1:
//                                 printf("   [%d] node_XY1 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY1.y);
//                                 break;
//                             case NodeOffsetPointXY_node_XY2:
//                                 printf("   [%d] node_XY2 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY2.y);
//                                 break;
//                             case NodeOffsetPointXY_node_XY3:
//                                 printf("   [%d] node_XY3 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY3.y);
//                                 break;
//                             case NodeOffsetPointXY_node_XY4:
//                                 printf("   [%d] node_XY4 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY4.y);
//                                 break;
//                             case NodeOffsetPointXY_node_XY5:
//                                 printf("   [%d] node_XY5 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY5.y);
//                                 break;
//                             case NodeOffsetPointXY_node_XY6:
//                                 printf("   [%d] node_XY6 x: %d, y: %d\n",
//                                 node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.x,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_XY6.y);
//                                 break;
//                             case NodeOffsetPointXY_node_LatLon:
//                                 printf("   [%d] LatLon latitude: %d,
//                                 longitude: %d\n", node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lat,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.u.node_LatLon.lon);
//                                 break;
//                             default:
//                                 printf("   [%d] Unhandled delta choice type:
//                                 %d\n", node_index,
//                                 map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.u.nodes.tab[node_index].delta.choice);
//                                 break;
//                         }
//                     }
//                 }
//                 else {
//                     printf("  Unhandled node choice type: %d\n",
//                     map->intersections.tab[intersection_index].laneSet.tab[lane_index].nodeList.choice);
//                 }
//                 if
//                 (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo_option)
//                 {
//                     printf("  ConnectsToList count: %d\n",
//                     map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count);
//                     for (connectionTo_index = 0; connectionTo_index <
//                     map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.count;
//                     connectionTo_index++) {
//                         printf("  ConnectsToList[%d]\n", connectionTo_index);
//                         if
//                         (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].remoteIntersection_option)
//                         {
//                             if
//                             (map->intersections.tab[intersection_index].id.region_option)
//                             {
//                                 printf("   remoteIntersection region: %d,",
//                                 map->intersections.tab[intersection_index].id.region);
//                             }
//                             printf(" id: %d, revision: %d\n",
//                             map->intersections.tab[intersection_index].id.id,
//                             map->intersections.tab[intersection_index].revision);
//                         }
//                         printf("   connectingLane lane: %d\n",
//                         map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.lane);
//                         if
//                         (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver_option)
//                         {
//                             printf("   connectingLane maneuver:");
//                             for (maneuver_index = 0; maneuver_index <
//                             AllowedManeuvers_MAX_BITS; maneuver_index++) {
//                                 if
//                                 (asn1_bstr_is_bit_set(&(map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].connectingLane.maneuver),
//                                 maneuver_index)) {
//                                     switch (maneuver_index) {
//                                         case
//                                         AllowedManeuvers_maneuverStraightAllowed:
//                                             printf("
//                                             maneuverStraightAllowed"); break;
//                                         case
//                                         AllowedManeuvers_maneuverLeftAllowed:
//                                             printf(" maneuverLeftAllowed");
//                                             break;
//                                         case
//                                         AllowedManeuvers_maneuverRightAllowed:
//                                             printf(" maneuverRightAllowed");
//                                             break;
//                                         default:
//                                             printf(" bit %d",
//                                             maneuver_index); break;
//                                     }
//                                 }
//                             }
//                             printf("\n");
//                         }
//                         if
//                         (map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup_option)
//                         {
//                             printf("   signalGroup: %d\n",
//                             map->intersections.tab[intersection_index].laneSet.tab[lane_index].connectsTo.tab[connectionTo_index].signalGroup);
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     return;
// }

/*void dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *)data;
    for (count = 0; count < len; count++) {
        if (count % 16 == 0)
            printf("\n");

        printf("%02X ", p[count]);
    }
    printf("\n\n");
}*/