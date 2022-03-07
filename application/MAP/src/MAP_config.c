#include "MAP_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "j2735_codec.h"
#include "j2735_msg.h"
#include "log.h"
#include "typedefine.h"

MAP_config_object_t MAP_config = {
    .MAP_packet_transfer_speed = 1,
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

void MAP_config_init(uint8_t **tx_buf, int *tx_buf_len)
{
    // MessageFrame msgf;
    // MapData *map;
    // J2735CodecErr j2735_err;

    // /* Make sure we reset the data structure at least once. */
    // memset(&msgf, 0, sizeof(msgf));
    // /* all fields which should be allocated before using are allocated
    // recursively */ map = (MapData *)j2735_msg_prealloc(MapData_Id);

    // map->timeStamp_option = FALSE;
    // map->msgIssueRevision = 0;
    // map->layerType_option = TRUE;
    // map->layerType = LayerType_intersectionData;
    // map->layerID_option = TRUE;
    // map->layerID = 1;
    // map->intersections_option = TRUE;

    // char log_content[LOG_CONTENT_LEN + 1];
    // memset(log_content, 0, sizeof(log_content));

    // FILE *fp;
    // fp = fopen(MAP_CONFIG_FILE, "r");
    // if(fp == NULL) {
    //     log_file_write_fatal_error("error opening %s", MAP_CONFIG_FILE);
    // } else {
    //     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
    //     strlen(log_content), "%s opened successfully", MAP_CONFIG_FILE);
    //     log_file_write(log_content);
    // }

    // char buf[CONFIG_LINE_BUFFER_SIZE];

    // uint8_t uint8_t_val;
    // float float_val;
    // char string_val[MAX_CONFIG_VARIABLE_LEN];
    // int intersection_number=0;
    // int intersection_count=1;
    // int intersection_laneSet_number=0;
    // int intersection_laneSet_count=1;
    // int intersection_speedLimits_count=1;
    // int intersection_speedLimits_number=0;
    // int intersection_laneSet_NodeXY_count=1;
    // int intersection_laneSet_NodeXY_number=0;
    // while (!feof(fp)) {
    //     memset(log_content, 0, sizeof(log_content));

    //     fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
    //     if (buf[0] == '#' || buf[0] == '\n' || buf[0] == ' ') {
    //         continue;
    //     }
    //     if (strstr(buf, "intersection_count ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.count = int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_n ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 intersection_number = int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_id ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].id.id =
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_revision ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].revision =
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_lat ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].refPoint.lat
    //                 = int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_long ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].refPoint.long
    //                 = int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_elevation ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].refPoint.elevation=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_lanWidth ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneWidth=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_speedLimits_count ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneWidth=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_speedLimits_n ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 intersection_speedLimits_number= int_val;
    //         }
    //     }
    //     if (strstr(buf, "IntersectionGeometry_speedLimits_type ")) {
    //         if (read_string_from_config_line(buf, string_val)) {

    //             strcpy(map->intersections.tab[intersection_number].speedLimits.tab[intersection_speedLimits_number].type,
    //             string_val);
    //         }
    //     }
    //     if (strstr(buf, "intersection_speedLimits_speed ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].speedLimits.tab[intersection_speedLimits_number].speed=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_count ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.count=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_n ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 intersection_laneSet_number= int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_id ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneID=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_ingressApproach ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].ingressApproach=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_egressApproach ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].egressApproach=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_laneAttributes_directionalUse
    //     ")) {
    //         if (read_string_from_config_line(buf, string_val)) {
    //                 asn1_bstr_alloc(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.directionalUse),
    //                 LaneDirection_MAX_BITS); for(int
    //                 i=0;i<LaneDirection_MAX_BITS;i++){
    //                     if(strcmp(string_val[i],"1")==1)
    //                         asn1_bstr_set_bit(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.directionalUse),
    //                         i);
    //                 }
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_laneAttributes_sharedWith ")) {
    //         if (read_string_from_config_line(buf, string_val)) {
    //                 asn1_bstr_alloc(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.sharedWith),
    //                 LaneSharing_MAX_BITS); for(int
    //                 i=0;i<LaneSharing_MAX_BITS;i++){
    //                     if(strcmp(string_val[i],"1")==1)
    //                         asn1_bstr_set_bit(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.sharedWith),
    //                         i);
    //                 }
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_laneAttributes_laneType ")) {
    //     //VEHICLE ONLY NOW
    //         if (read_string_from_config_line(buf, string_val)) {
    //                 asn1_bstr_alloc(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.laneType.u.vehicle),
    //                 LaneAttributes_Vehicle_MAX_BITS); for(int
    //                 i=0;i<LaneAttributes_Vehicle_MAX_BITS;i++){
    //                     if(strcmp(string_val[i],"1")==1)
    //                         asn1_bstr_set_bit(&(map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].laneAttributes.laneType.u.vehicle),
    //                         i);
    //                 }
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_NodeXY_count ")) { //CHOICE
    //     SHOULD ADD
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.choice
    //                 = NodeListXY_nodes;
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.count=
    //                 int_val;
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab
    //                 = (NodeXY *)calloc(3, sizeof(NodeXY));
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_NodeXY_n ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 intersection_laneSet_NodeXY_number= int_val;
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.choice
    //                 = NodeOffsetPointXY_node_XY6;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_NodeXY_X ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_XY6.x=
    //                 int_val;
    //         }
    //     }
    //     if (strstr(buf, "intersection_laneSet_NodeXY_Y ")) {
    //         if (read_int_from_config_line(buf, &int_val)) {
    //                 map->intersections.tab[intersection_number].laneSet.tab[intersection_laneSet_number].nodeList.u.nodes.tab[intersection_laneSet_NodeXY_number].delta.u.node_XY6.y=
    //                 int_val;
    //         }
    //     }
    //     //MAP_packet_transfer_speed
    //     if (strstr(buf, "MAP_packet_transfer_speed ")) {
    //         if (read_uint8_t_from_config_line(buf, &uint8_t_val)) {
    //             if (uint8_t_val >= 0) {
    //                 MAP_config.MAP_packet_transfer_speed = uint8_t_val;
    //                 snprintf(log_content + strlen(log_content),
    //                 LOG_CONTENT_LEN - strlen(log_content), "config:
    //                 MAP_packet_transfer_speed = %d",
    //                 MAP_config.MAP_packet_transfer_speed);
    //                 log_file_write(log_content);
    //                 continue;
    //             } else {
    //                 return CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
    //             }
    //         } else {
    //             return CONFIG_INVALID_MAP_PACKET_TRANSFER_SPEED;
    //         }
    //     }
    // }
    // //lanelist end
    // map->intersections.tab[0].preemptPriorityData_option = FALSE;
    // map->intersections.tab[0].regional_option = FALSE;

    // map->roadSegments_option = FALSE;
    // map->dataParameters_option = FALSE;
    // map->restrictionList_option = FALSE;
    // map->regional_option = FALSE;

    // /* does the encoding and providing the error msg if there is */
    // msgf.messageId = MapData_Id;
    // msgf.u.data = map;
    // j2735_err.msg_size = 256;
    // j2735_err.msg = malloc(256);
    // *tx_buf_len = j2735_msg_encode(tx_buf, &msgf, &j2735_err);
    // if (*tx_buf_len <= 0) {
    //     printf("failed to encode the msg\n");
    //     printf("encode err: %s\n", j2735_err.msg);
    // }
    // else {
    //     printf("encode successfully\n");
    // }

    // /* free the memory for encoding */
    // for (int i = 0; i < map->intersections.tab[0].laneSet.count; i++) {
    //     asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].laneAttributes.directionalUse));
    //     asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].laneAttributes.sharedWith));
    //     if (i < 8) {
    //         asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].laneAttributes.laneType.u.vehicle));
    //         if (0 == (i % 2)) {
    //             if
    //             (map->intersections.tab[0].laneSet.tab[i].connectsTo_option)
    //             {
    //                 asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].connectsTo.tab[0].connectingLane.maneuver));
    //                 asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].connectsTo.tab[1].connectingLane.maneuver));
    //                 asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].connectsTo.tab[2].connectingLane.maneuver));
    //             }
    //         }
    //     }
    //     else {
    //         asn1_bstr_free(&(map->intersections.tab[0].laneSet.tab[i].laneAttributes.laneType.u.crosswalk));
    //     }
    // }
    // j2735_msg_dealloc(MapData_Id, map);
    // free(j2735_err.msg);


    // fclose(fp);
    // // return MAP_CONFIG_ACCEPT;
    // return;
}