#ifndef EXTERNAL_APP_PROXY_INNER_H
#define EXTERNAL_APP_PROXY_INNER_H

#include <stdio.h>
#include <stdint.h>

#include "typedefine.h"
#include "traffic_signal_command_buffer.h"
#include "vms.h"
#include "ObstacleList.h"
#include "log.h"
#include "external_app_proxy_typedefine.h"

/* since application might implement multi-thread program, ,
 * we add a mutex_lock to serialize their usage of the same channel */ 
/* these mutex should only be used in library function provided for external app client */
extern pthread_mutex_t mutex_notify_fd;
extern pthread_mutex_t mutex_interact_fd;

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

typedef struct _packet_from_proxy_header_t {
    uint32_t packet_type;
    uint32_t seq_num;   //used match heartbeat seq_num
    union{
        event_type_t callback_event; //used for notify
        int ret_val; //used for ack
    };
} packet_from_proxy_header_t;

typedef struct _packet_to_proxy_header_t {
    uint32_t packet_type;
    pid_t pid;
    uint32_t appID;
    union{
        uint32_t api_id;    //used for api request
        uint32_t seq_num;   //used for send heartbeat
    };
} packet_to_proxy_header_t;

typedef struct _app_registration_payload_t {
    char name[APP_NAME_MAX_LEN];
    uint8_t id;
    uint8_t priority;
    uint8_t dontSend2TC;
    uint64_t callback_register_mask;
    pid_t pid;
} app_registration_payload_t;

typedef struct _notify_update_payload_t {
    uint8_t id;
    pid_t pid;
} notify_update_payload_t;

typedef struct _notify_update_ack_payload_t {
    uint32_t RSU_id;
    double RSU_lat;
    double RSU_lon;
    char RSU_name[RSU_NAME_MAX_LEN];
    double RSU_elev;
    uint32_t RSU_region;
} notify_update_ack_payload_t;

#define API_ID_OF(api_name)  api_name ## _API_ID
enum ea_callback_api_id_definition_enum{
    /* since '0' is a special number, we reserve it for future expansion */
    API_ID_OF(special_reserved_api_id) = 0,
    
    /* external_app_proxy.h */
    API_ID_OF(remote_app_registration),
    API_ID_OF(app_main_loop_start),

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

extern char* api_id_str_arr[NUM_OF_API_ID_DEFININITION];

/* functions below are only used in library used by external application */
/* middleware itself will not use */
void set_register_app_id(uint32_t appID);
uint32_t get_register_app_id();

// inline int32_t simple_send_request_header_to_proxy(int api_id);
// inline int32_t simple_get_ack_from_proxy(packet_from_proxy_header_t *ack_p, char* api_name);

int32_t is_interact_fd_linked();
int32_t interact_fd_disconnect_from_proxy();
int32_t interact_fd_connect_to_proxy();
int32_t interact_fd_recv_from_proxy(void* packet_p, size_t packet_size);
int32_t interact_fd_send_to_proxy(void* packet_p, size_t packet_size);

int32_t add_notify_fd_to_epoll(int* ep_fd);
int32_t is_notify_fd_linked();
int32_t notify_fd_disconnect_from_proxy();
int32_t notify_fd_connect_to_proxy();
int32_t notify_fd_recv_from_proxy(void* packet_p, size_t packet_size);
int32_t notify_fd_send_to_proxy(void* packet_p, size_t packet_size);
/* functions above ... */

inline __attribute__((always_inline))  
int32_t simple_send_request_header_to_proxy(int api_id) 
{   
    int ret;
    packet_to_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_REQ;
    header.appID = get_register_app_id();
    header.api_id = api_id;
    ret = interact_fd_send_to_proxy(&header, sizeof(header));
    log_file_write("%s: send header to proxy, ret = %d", api_id_str_arr[api_id], ret);
    if(ret!=0){
        log_file_write_fatal_error("%s: interact_fd_send_to_proxy ret != 0\n");
    }
    return ret;
}

inline __attribute__((always_inline)) 
int32_t simple_get_ack_from_proxy(packet_from_proxy_header_t *ack_p, char* api_name)
{
    int ret;
    memset(ack_p, 0, sizeof(packet_from_proxy_header_t));
    ret = interact_fd_recv_from_proxy(ack_p, sizeof(packet_from_proxy_header_t));
    log_file_write("%s: get ack from proxy, ret = %d", api_name, ret);
    if(ret!=0){
        log_file_write_fatal_error("%s: interact_fd_recv_from_proxy ret != 0\n");
    }
    else if( ack_p->packet_type != EA_PACKET_TYPE_ACK ){
        log_file_write("%s: ack packet not EA_PACKET_TYPE_ACK\n");
        ret = EA_ERR_PACKET_TYPE_NOT_MATCH;
    }
    else{
        ret = EA_ERR_OK;
    }
    return ret;
}

#endif  /* EXTERNAL_APP_PROXY_INNER_H */