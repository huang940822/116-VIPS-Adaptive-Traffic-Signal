#ifndef EAP_INNER_IMPLEMENTATION_H
#define EAP_INNER_IMPLEMENTATION_H

#include <stdio.h>
#include <stdint.h>

#include "ea_external_app_proxy.h"
#include "ea_traffic_signal_command_buffer.h"

int32_t connect_to_proxy();
int32_t disconnect_from_proxy();
int32_t send_to_proxy( void* req_packet_p, size_t packet_size);
int32_t read_from_proxy( void* ret_packet_p, size_t packet_size);

#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"

enum ea_packet_type_define_enum{
    EA_PACKET_TYPE_RESERVED = 0,    /*reserved*/
    EA_PACKET_TYPE_REQ,     /*requeset*/
    EA_PACKET_TYPE_ACK,     /*ack*/
    EA_PACKET_TYPE_REG,     /*register*/
    EA_PACKET_TYPE_NM_NTF,  /*normal notify*/
    EA_PACKET_TYPE_SP_NTF,  /*special notify*/
    EA_PACKET_TYPE_HEARTB,  /*heartbeat*/

    /* this tag should always be at the last*/
    NUM_OF_EA_PACKET_TYPE,  
};

#define API_ID_OF(api_name) API_ID_ ## api_name
#define API_ID_OF(api_name) API_ID_ ## api_name

enum api_id_define_enum{
    /* since '0' is a special number, we reserve it for future expansion */
    API_ID_OF(special_reserved_id) = 0,
    
    /* ea_application_registration.h */
    API_ID_OF(event_callback_msg_id_insert),
    
    /* ea_com_packet_processing.h */
    API_ID_OF(cloud_packet_tx),
    // API_ID_OF(OBU_packet_tx),
    // API_ID_OF(OBU_j2735_tx),
    API_ID_OF(remote_com_send_OBU),
    
    /* ea_config.h */
    API_ID_OF(get_config_RSU_id),
    API_ID_OF(get_config_RSU_lat),
    API_ID_OF(get_config_RSU_lon),
    API_ID_OF(get_config_RSU_name),
    API_ID_OF(get_config_RSU_region),
    API_ID_OF(get_config_RSU_elev),
    
    /* ea_traffic_signal_command_buffer.h */
    API_ID_OF(command_buf_insert_effect_time),
    API_ID_OF(command_buf_insert_adjustment),
    
    /* ea_vms.h */
    API_ID_OF(vms_request_start),
    API_ID_OF(vms_request_end),

    /* ea_traffic_signal_status_updating.h */
    API_ID_OF(get_traffic_signal_status),
    API_ID_OF(get_current_traffic_signal_status),
    API_ID_OF(get_current_phase),
    API_ID_OF(get_current_step),
    API_ID_OF(get_current_second),
    API_ID_OF(get_SubPhaseCount),
    API_ID_OF(get_SignalCount),
    API_ID_OF(get_plan_id),
    API_ID_OF(get_control_status),
    API_ID_OF(get_PhaseOrder),
    API_ID_OF(get_remaining_time),
    API_ID_OF(get_SignalStatus),
    API_ID_OF(get_total_compensation_second),
    API_ID_OF(get_compensation_buffer),
    API_ID_OF(set_control_status),
    API_ID_OF(get_original_tc_health_status),
    API_ID_OF(get_next_SubPhaseID),
    API_ID_OF(get_prev_SubPhaseID),

    /* this tag should always be at the last*/
    NUM_OF_API_ID,  
};

// #define API_ID_OF(special_reserved) 0
// #define API_ID_OF(event_callback_msg_id_insert) 1
// #define API_ID_OF(cloud_packet_tx) 2
// #define API_ID_OF(OBU_packet_tx) 3
// #define API_ID_OF(OBU_j2735_tx) 4
// #define API_ID_OF(remote_com_send_OBU) 5

#define REQ_PACKET_TYPE(api_name) _## api_name ## _req_packet_t
#define ACK_PACKET_TYPE(api_name) _## api_name ## _ack_packet_t

struct REQ_PACKET_TYPE(event_callback_msg_id_insert){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        event_type_t event_type;
        char name[APP_NAME_MAX_LEN];
        int priority;
        DSRCmsgID msg_id;
        //int (*callback)(void *);  //middleware will use a special CB
    } payload;
};
struct ACK_PACKET_TYPE(event_callback_msg_id_insert){
    uint32_t packet_type;
    int ret_val;
}ack_packet;

struct REQ_PACKET_TYPE(cloud_packet_tx){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint16_t len;
        uint8_t service_id;
        unsigned char* specific_field_p;
    } payload;
};
struct ACK_PACKET_TYPE(cloud_packet_tx){
    uint32_t packet_type;
    int ret_val;
};

struct REQ_PACKET_TYPE(remote_com_send_OBU){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        int buf_len;
        uint8_t *buf;
    } payload;
};
struct ACK_PACKET_TYPE(remote_com_send_OBU){
    uint32_t packet_type;
    int ret_val;
};

struct REQ_PACKET_TYPE(get_config_RSU_id){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_id){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint32_t RSU_id;
    } payload;
};

struct REQ_PACKET_TYPE(get_config_RSU_lat){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_lat){
    uint32_t packet_type;
    int ret_val;
    struct {
        double RSU_lat;
    } payload;
};

struct REQ_PACKET_TYPE(get_config_RSU_lon){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_lon){
    uint32_t packet_type;
    int ret_val;
    struct {
        double RSU_lon;
    } payload;
};

struct REQ_PACKET_TYPE(get_config_RSU_name){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_name){
    uint32_t packet_type;
    int ret_val;
    struct {
        char RSU_name_arr[RSU_NAME_MAX_LEN];
    } payload;
};

struct REQ_PACKET_TYPE(get_config_RSU_region){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_region){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint32_t RSU_region;
    } payload;
};

struct REQ_PACKET_TYPE(get_config_RSU_elev){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_config_RSU_elev){
    uint32_t packet_type;
    int ret_val;
    struct {
        double RSU_elev;
    } payload;
};

struct REQ_PACKET_TYPE(command_buf_insert_effect_time){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        tsc_command_t tsc_cmd;
    } payload;
};
struct ACK_PACKET_TYPE(command_buf_insert_effect_time){
    uint32_t packet_type;
    int ret_val;
};

struct REQ_PACKET_TYPE(command_buf_insert_adjustment){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        tsc_command_t tsc_cmd;
    } payload;
};
struct ACK_PACKET_TYPE(command_buf_insert_adjustment){
    uint32_t packet_type;
    int ret_val;
};

struct REQ_PACKET_TYPE(vms_request_start){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint8_t id;
        uint8_t priority;
    } payload;
};
struct ACK_PACKET_TYPE(vms_request_start){
    uint32_t packet_type;
    int ret_val;
}ack_packet;

struct REQ_PACKET_TYPE(vms_request_end){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint8_t id;
    } payload;
};
struct ACK_PACKET_TYPE(vms_request_end){
    uint32_t packet_type;
    int ret_val;
}ack_packet;

struct REQ_PACKET_TYPE(get_traffic_signal_status){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_traffic_signal_status){
    uint32_t packet_type;
    int ret_val;
    struct {
        traffic_signal_status_t ts_status;
    } payload;
};

struct REQ_PACKET_TYPE(get_current_traffic_signal_status){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_current_traffic_signal_status){
    uint32_t packet_type;
    int ret_val;
    struct {
        traffic_signal_status_t cur_ts_status;
    } payload;
};

struct REQ_PACKET_TYPE(get_current_phase){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_current_phase){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t phase;
    } payload;
};

struct REQ_PACKET_TYPE(get_current_step){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_current_step){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t step;
    } payload;
};

struct REQ_PACKET_TYPE(get_current_second){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_current_second){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint16_t second;
    } payload;
};

struct REQ_PACKET_TYPE(get_SubPhaseCount){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_SubPhaseCount){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t SubPhaseCount;
    } payload;
};

struct REQ_PACKET_TYPE(get_SignalCount){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_SignalCount){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t SignalCount;
    } payload;
};

struct REQ_PACKET_TYPE(get_plan_id){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_plan_id){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t plan_id;
    } payload;
};

struct REQ_PACKET_TYPE(get_control_status){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_control_status){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t ctrl_status;
    } payload;
};

struct REQ_PACKET_TYPE(get_PhaseOrder){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_PhaseOrder){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t PhaseOrder;
    } payload;
};

struct REQ_PACKET_TYPE(get_remaining_time){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint8_t phase;
        uint8_t step;
        uint16_t second;
    } payload;
};
struct ACK_PACKET_TYPE(get_remaining_time){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint16_t remaining_time;
    } payload;
};

struct REQ_PACKET_TYPE(get_SignalStatus){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint8_t SubPhaseCount_index;
        uint8_t SignalCount_index;
    } payload;
};
struct ACK_PACKET_TYPE(get_SignalStatus){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t SignalStatus;
    } payload;
};

struct REQ_PACKET_TYPE(get_total_compensation_second){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_total_compensation_second){
    uint32_t packet_type;
    int ret_val;
    struct {
        int16_t total_cps_sec;
    } payload;
};

struct REQ_PACKET_TYPE(get_compensation_buffer){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_compensation_buffer){
    uint32_t packet_type;
    int ret_val;
    struct {
        int16_t compensation_buffer[SUBPHASEID_NUM];
    } payload;
};

struct REQ_PACKET_TYPE(set_control_status){
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        uint8_t control_status;
    } payload;
};
struct ACK_PACKET_TYPE(set_control_status){
    uint32_t packet_type;
    int ret_val;
};

struct REQ_PACKET_TYPE(get_original_tc_health_status){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_original_tc_health_status){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint16_t ori_tc_health_status;
    } payload;
};

struct REQ_PACKET_TYPE(get_next_SubPhaseID){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_next_SubPhaseID){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t next_SubPhaseID;
    } payload;
};

struct REQ_PACKET_TYPE(get_prev_SubPhaseID){
    uint32_t packet_type;
    uint32_t api_id;
};
struct ACK_PACKET_TYPE(get_prev_SubPhaseID){
    uint32_t packet_type;
    int ret_val;
    struct {
        uint8_t prev_SubPhaseID;
    } payload;
};

#endif  /* EAP_INNER_IMPLEMENTATION_H */