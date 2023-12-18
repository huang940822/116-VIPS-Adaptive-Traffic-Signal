#ifndef EXTERNAL_APP_PROXY_SOCKET_H
#define EXTERNAL_APP_PROXY_SOCKET_H

#include <stdio.h>
#include <stdint.h>

#include "typedefine.h"
#include "traffic_signal_command_buffer.h"
#include "vms.h"
#include "ObstacleList.h"
#include "log.h"
#include "external_app_proxy_typedefine.h"

/* DANGER: this file: external_app_proxy_socket.h
 * will exist both is server-side and client-side
 * the two should be "THE SAME" !! */

#define USING_NON_ACK_CONFIG 1

enum ea_packet_type_definition_enum{
    EA_PACKET_TYPE_RESERVED = 0,    /*reserved, for future expansion, such as emergency handling*/
    EA_PACKET_TYPE_REGISTER_TOP,    /*register top-half, will establish intetaction channel */
    EA_PACKET_TYPE_REGISTER_BOT,    /*register top-half, will establish notification channel */
    EA_PACKET_TYPE_REQ,             /*request*/
    EA_PACKET_TYPE_ACK,             /*ack of request */
    EA_PACKET_TYPE_NM_NTF,          /*normal notify, i.e., EVENT message send to EA */
    EA_PACKET_TYPE_SP_NTF,          /*special notify, for future expansion, currently not in use*/
    EA_PACKET_TYPE_REPORT,          /*for sending heartbeat*/
    
    /* tag below should always be at the last*/
    TOTAL_NUM_OF_EA_PACKET_TYPE,  
};

// typedef struct _proxy_packet_header_t {
//     uint32_t packet_type;
//     union{
//         /* register type API header */
//         struct {
//             uint32_t api_id;     //used when registering
//         }reg_h;

//         /* event type API header*/
//         struct {
//             event_type_t callback_event; //event id
//             struct{     //for test
//                 long glb_sec;
//                 long glb_nsec;
//             };
//         }evt_h; 

//         /* request (get/set) type API header */
//         struct {
//             uint32_t appID;
//             union{
//                 uint32_t api_id;    //used when send api-request
//                 int ret_val;        //return value of api-request
//             };
//         }req_h; 

//         /* report type API header */
//         struct {
//             uint32_t appID;     //used for heartbeat packet
//         }rpt_h; 
//     };
// }proxy_packet_header_t;

// typedef proxy_packet_header_t packet_header_from_proxy_t;
// typedef proxy_packet_header_t packet_header_to_proxy_t;

typedef struct _packet_header_from_proxy_t {
    uint32_t packet_type;
    union{
        event_type_t callback_event; //event id
        int ret_val;        //return value of api-request
    };
    struct{
        long glb_sec;
        long glb_nsec;
    };
} packet_header_from_proxy_t;

typedef struct _packet_header_to_proxy_t {
    uint32_t packet_type;
    uint32_t api_id;    //used when registering or send api-request
    uint32_t appID;     //used when sending heartbeat
} packet_header_to_proxy_t;

#define API_ID_OF(api_name)  api_name ## _API_ID
enum ea_callback_api_id_definition_enum{
    /* since '0' is a special number, we reserve it for future expansion */
    API_ID_OF(special_reserved_api_id) = 0,
    API_ID_OF(reserved_id_for_heartbeat_comm),
    
    /* external_app_proxy.h */
    API_ID_OF(app_remote_register),

    /* application_registration.h */
    API_ID_OF(event_callback_msg_id_insert),
    
    /* com_packet_processing.h */
    API_ID_OF(cloud_packet_tx),
    API_ID_OF(OBU_j2735_tx),
    API_ID_OF(OBU_packet_tx),
    API_ID_OF(remote_com_send),
    
    /* config.h */
    API_ID_OF(get_config_RSU_id),
    API_ID_OF(get_config_RSU_lat),
    API_ID_OF(get_config_RSU_lon),
    API_ID_OF(get_config_RSU_name),
    API_ID_OF(get_config_RSU_region),
    API_ID_OF(get_config_RSU_elev),
    
    /* traffic_signal_command_buffer.h */
    API_ID_OF(command_buf_insert_effect_time),
    API_ID_OF(command_buf_insert_adjustment),
    
    /* vms.h */
    API_ID_OF(vms_request_start),
    API_ID_OF(vms_request_end),
    API_ID_OF(vms_sync_evsp_prog),
    API_ID_OF(vms_sync_then_start),
    
    /* traffic_signal_status_updating.h */
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

extern char* api_id_str_arr[];
char* get_str_by_err_code(int err_code);

/* since application might implement multi-thread program, ,
 * we add a mutex_lock to serialize their usage of the same channel 
 * the implementation is based on mutex lock */ 
void lock_interact_channel();
void unlock_interact_channel();
void lock_notify_channel();
void unlock_notify_channel();

/* functions below are only used in library used by external application */
/* middleware itself will not use */
/* recommend middleware-api implementation use functions below  */
bool is_proxy_connected();
int32_t simple_send_request_header_to_proxy(int api_id);
int32_t simple_send_heartbeat_to_proxy();
int32_t simple_send_packet_to_proxy(void* packet_p, size_t packet_size);
int32_t simple_recv_ack_header_from_proxy(packet_header_from_proxy_t *ack_p, int api_id);
int32_t simple_recv_packet_from_proxy(void* packet_p, size_t packet_size);
/* functions above ... */


#endif  /* EXTERNAL_APP_PROXY_SOCKET_H */