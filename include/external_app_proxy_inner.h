#ifndef EXTERNAL_APP_PROXY_INNER_H
#define EXTERNAL_APP_PROXY_INNER_H

#include <stdio.h>
#include <stdint.h>

#include "typedefine.h"
#include "traffic_signal_command_buffer.h"
#include "vms.h"

/* functions below are only used in library used by external application */
/* middleware itself will not use */
int32_t is_interact_fd_linked();
int32_t interact_fd_disconnect_from_proxy();
int32_t interact_fd_connect_to_proxy();
int32_t interact_fd_recv_from_proxy(void* packet_p, size_t packet_size);
int32_t interact_fd_send_to_proxy(void* packet_p, size_t packet_size);

int32_t is_notify_fd_linked();
int32_t notify_fd_disconnect_from_proxy();
int32_t notify_fd_connect_to_proxy();
int32_t notify_fd_recv_from_proxy(void* packet_p, size_t packet_size);
int32_t notify_fd_send_to_proxy(void* packet_p, size_t packet_size);
/* functions above ... */

enum ea_packet_type_definition_enum{
    EA_PACKET_TYPE_RESERVED = 0,    /*reserved*/
    EA_PACKET_TYPE_REGISTER,        /*register, will update intetact channel */
    EA_PACKET_TYPE_NTF_UPDATE,      /*notify channel update*/
    EA_PACKET_TYPE_REQ,             /*requeset*/
    EA_PACKET_TYPE_ACK,             /*ack*/
    EA_PACKET_TYPE_NM_NTF,          /*normal notify*/
    EA_PACKET_TYPE_SP_NTF,          /*special notify*/
    EA_PACKET_TYPE_HEARTBEAT,       /*heartbeat*/
    EA_PACKET_TYPE_PROXY,           /*current version not used yet, used by proxy library for special usage*/
    EA_PACKET_TYPE_PING,            /*current version not used yet, used by proxy library for ping testing*/
    /* this tag should always be at the last*/
    NUM_OF_EA_PACKET_TYPE_DEFININITION,  
};

// #define BIT_SHIFT_FOR(callback_name) BIT_SHIFT_ ## callback_name
// enum ea_callback_func_bit_shift_definition_enum{
//     /* currently using the matching definition from enum event_type */
//     BIT_SHIFT_FOR(on_OBU_packet_rx) = EVENT_OBU_PACKET_RX, 
//     BIT_SHIFT_FOR(on_OBU_packet_tx) = EVENT_OBU_PACKET_TX,
//     BIT_SHIFT_FOR(on_RSU_packet_rx) = EVENT_RSU_PACKET_RX, 
//     BIT_SHIFT_FOR(on_RSU_packet_tx) = EVENT_RSU_PACKET_TX,
//     BIT_SHIFT_FOR(on_cloud_packet_rx) = EVENT_CLOUD_PACKET_RX,
//     BIT_SHIFT_FOR(on_cloud_packet_tx) = EVENT_CLOUD_PACKET_TX,
//     BIT_SHIFT_FOR(on_traffic_signal_command_tx) = EVENT_TRAFFIC_SIGNAL_COMMAND_TX,
//     BIT_SHIFT_FOR(on_camera_packet_rx) = EVENT_CAMERA_PACKET_RX,
//     BIT_SHIFT_FOR(on_registration) = EVENT_REGISTRATION, 
//     BIT_SHIFT_FOR(on_middleware_restart) = EVENT_MIDDLEWARE_RESTART,

//     /* this tag should always be at the last*/
//     NUM_OF_BIT_SHIFT_DEFININITION,  
// };

#define REGI_BIT(callback_name) callback_name ## _regi_bit
typedef struct _callback_register_mask_t {
    unsigned REGI_BIT(on_OBU_packet_rx): 1;
    unsigned REGI_BIT(on_OBU_packet_tx): 1;
    unsigned REGI_BIT(on_RSU_packet_rx): 1;
    unsigned REGI_BIT(on_RSU_packet_tx): 1;
    unsigned REGI_BIT(on_cloud_packet_rx): 1;
    unsigned REGI_BIT(on_cloud_packet_tx): 1;
    unsigned REGI_BIT(on_traffic_signal_command_tx): 1;
    unsigned REGI_BIT(on_camera_packet_rx): 1;
    unsigned REGI_BIT(on_registration): 1;
    unsigned REGI_BIT(on_middleware_restart): 1;
} callback_regi_mask_t;

typedef struct _packet_from_proxy_header_t {
    uint32_t packet_type;
    event_type_t callback_event;
}packet_from_proxy_header_t;

typedef struct _ack_from_proxy_header_t {
    uint32_t packet_type;
    int ret_val;
    //uint32_t payload_len;   /* current version do not need */
}ack_from_proxy_header_t;

typedef struct _packet_to_proxy_header_t {
    uint32_t packet_type;
    uint32_t api_id;
    //uint32_t payload_len;     /* current version do not need */
}packet_to_proxy_header_t;

typedef struct _packet_to_proxy_hearbeat_t {
    uint32_t appID;
}packet_to_proxy_hearbeat_t;



#define API_ID_OF(api_name) API_ID_ ## api_name
enum ea_callback_api_id_definition_enum{
    /* since '0' is a special number, we reserve it for future expansion */
    API_ID_OF(special_reserved_id) = 0,
    
    /* ea_external_app_proxy.h */
    API_ID_OF(remote_app_registration),
    API_ID_OF(app_main_loop_start),

    /* ea_application_registration.h */
    API_ID_OF(event_callback_msg_id_insert),
    
    /* ea_com_packet_processing.h */
    API_ID_OF(cloud_packet_tx),
    API_ID_OF(OBU_j2735_tx),
    API_ID_OF(OBU_packet_tx),
    API_ID_OF(remote_com_send),
    
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
    API_ID_OF(vms_sync_evsp_prog),
    
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
    NUM_OF_API_ID_DEFININITION,  
};






















#define REQ_PAYLOAD_TYPE(api_name) _## api_name ## _req_payload_t
#define ACK_PAYLOAD_TYPE(api_name) _## api_name ## _ack_payload_t

struct REQ_PAYLOAD_TYPE(remote_app_registration){
    char name[APP_NAME_MAX_LEN];
    uint8_t id;
    uint8_t priority;
    uint8_t dontSend2TC;
    uint32_t callback_register_mask;
    pid_t pid;
};
struct ACK_PAYLOAD_TYPE(remote_app_registration){
    uint32_t RSU_id;
    double RSU_lat;
    double RSU_lon;
    char RSU_name[RSU_NAME_MAX_LEN];
    double RSU_elev;
    uint32_t RSU_region;
};

struct REQ_PAYLOAD_TYPE(event_callback_msg_id_insert){
    event_type_t event_type;
    char name[APP_NAME_MAX_LEN];
    int priority;
    DSRCmsgID msg_id;
    //int (*callback)(void *);  /* current version no need to send this */
};
struct ACK_PAYLOAD_TYPE(event_callback_msg_id_insert){
    ;
};

struct REQ_PAYLOAD_TYPE(cloud_packet_tx){
    uint16_t len;
    uint8_t service_id;
};
struct ACK_PAYLOAD_TYPE(cloud_packet_tx){
    ;
};

struct REQ_PAYLOAD_TYPE(OBU_j2735_tx){
    int buf_len;
};
struct ACK_PAYLOAD_TYPE(OBU_j2735_tx){
    ;
};

struct REQ_PAYLOAD_TYPE(OBU_packet_tx){
    uint16_t write_buf_len;
};
struct ACK_PAYLOAD_TYPE(OBU_packet_tx){
    ;
};

struct REQ_PAYLOAD_TYPE(remote_com_send){
    int buf_len;
};
struct ACK_PAYLOAD_TYPE(remote_com_send){
    ;
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_id){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_id){
    uint32_t RSU_id;
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_lat){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_lat){
    double RSU_lat;
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_lon){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_lon){
    double RSU_lon;
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_name){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_name){
    char RSU_name_arr[RSU_NAME_MAX_LEN];
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_region){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_region){
    uint32_t RSU_region;
};

struct REQ_PAYLOAD_TYPE(get_config_RSU_elev){
    ;
};
struct ACK_PAYLOAD_TYPE(get_config_RSU_elev){
    double RSU_elev;
};

struct REQ_PAYLOAD_TYPE(command_buf_insert_effect_time){
    tsc_command_t tsc_cmd;
};
struct ACK_PAYLOAD_TYPE(command_buf_insert_effect_time){
    ;
};

struct REQ_PAYLOAD_TYPE(command_buf_insert_adjustment){
    tsc_command_t tsc_cmd;
};
struct ACK_PAYLOAD_TYPE(command_buf_insert_adjustment){
    ;
};

struct REQ_PAYLOAD_TYPE(vms_request_start){
    uint8_t id;
    uint8_t priority;
};
struct ACK_PAYLOAD_TYPE(vms_request_start){
    ;
};

struct REQ_PAYLOAD_TYPE(vms_request_end){
    uint8_t id;
};
struct ACK_PAYLOAD_TYPE(vms_request_end){
    ;
};

struct REQ_PAYLOAD_TYPE(vms_sync_evsp_prog){
    uint8_t evsp_prog[RTM_MAX];
};
struct ACK_PAYLOAD_TYPE(vms_sync_evsp_prog){
    ;
};

struct REQ_PAYLOAD_TYPE(get_traffic_signal_status){
    ;
};
struct ACK_PAYLOAD_TYPE(get_traffic_signal_status){
    traffic_signal_status_t ts_status;
};

struct REQ_PAYLOAD_TYPE(get_current_traffic_signal_status){
    ;
};
struct ACK_PAYLOAD_TYPE(get_current_traffic_signal_status){
    traffic_signal_status_t cur_ts_status;
};

struct REQ_PAYLOAD_TYPE(get_current_phase){
    ;
};
struct ACK_PAYLOAD_TYPE(get_current_phase){
    uint8_t phase;
};

struct REQ_PAYLOAD_TYPE(get_current_step){
    ;
};
struct ACK_PAYLOAD_TYPE(get_current_step){
    uint8_t step;
};

struct REQ_PAYLOAD_TYPE(get_current_second){
    ;
};
struct ACK_PAYLOAD_TYPE(get_current_second){
    uint16_t second;
};

struct REQ_PAYLOAD_TYPE(get_SubPhaseCount){
    ;
};
struct ACK_PAYLOAD_TYPE(get_SubPhaseCount){
    uint8_t SubPhaseCount;
};

struct REQ_PAYLOAD_TYPE(get_SignalCount){
    ;
};
struct ACK_PAYLOAD_TYPE(get_SignalCount){
    uint8_t SignalCount;
};

struct REQ_PAYLOAD_TYPE(get_plan_id){
    ;
};
struct ACK_PAYLOAD_TYPE(get_plan_id){
    uint8_t plan_id;
};

struct REQ_PAYLOAD_TYPE(get_control_status){
    ;
};
struct ACK_PAYLOAD_TYPE(get_control_status){
    uint8_t ctrl_status;
};

struct REQ_PAYLOAD_TYPE(get_PhaseOrder){
    ;
};
struct ACK_PAYLOAD_TYPE(get_PhaseOrder){
    uint8_t PhaseOrder;
};

struct REQ_PAYLOAD_TYPE(get_remaining_time){
    uint8_t phase;
    uint8_t step;
    uint16_t second;
};
struct ACK_PAYLOAD_TYPE(get_remaining_time){
    uint16_t remaining_time;
};

struct REQ_PAYLOAD_TYPE(get_SignalStatus){
    uint8_t SubPhaseCount_index;
    uint8_t SignalCount_index;
};
struct ACK_PAYLOAD_TYPE(get_SignalStatus){
    uint8_t SignalStatus;
};

struct REQ_PAYLOAD_TYPE(get_total_compensation_second){
    ;
};
struct ACK_PAYLOAD_TYPE(get_total_compensation_second){
    int16_t total_cps_sec;
};

struct REQ_PAYLOAD_TYPE(get_compensation_buffer){
    ;
};
struct ACK_PAYLOAD_TYPE(get_compensation_buffer){
    int16_t compensation_buffer[SUBPHASEID_NUM];
};

struct REQ_PAYLOAD_TYPE(set_control_status){
    uint8_t control_status;
};
struct ACK_PAYLOAD_TYPE(set_control_status){
    ;
};

struct REQ_PAYLOAD_TYPE(get_original_tc_health_status){
    ;
};
struct ACK_PAYLOAD_TYPE(get_original_tc_health_status){
    uint16_t ori_tc_health_status;
};

struct REQ_PAYLOAD_TYPE(get_next_SubPhaseID){
    ;
};
struct ACK_PAYLOAD_TYPE(get_next_SubPhaseID){
    uint8_t next_SubPhaseID;
};

struct REQ_PAYLOAD_TYPE(get_prev_SubPhaseID){
    ;
};
struct ACK_PAYLOAD_TYPE(get_prev_SubPhaseID){
    uint8_t prev_SubPhaseID;
};

#endif  /* EXTERNAL_APP_PROXY_INNER_H */