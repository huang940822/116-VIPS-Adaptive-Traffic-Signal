#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "log.h"
#include "typedefine.h"
#include "application_registration.h"
#include "com_packet_processing.h"
#include "config.h"
#include "traffic_signal_command_buffer.h"
#include "vms.h"
#include "traffic_signal_status_updating.h"
#include "com_io.h"

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_api_wrapper.h"
#include "external_app_proxy_callback_wrapper.h"

#define WRAPPER_FUNC_OF(api_name) api_name ## api_wrapper

/* application_registration.h */
int WRAPPER_FUNC_OF(event_callback_msg_id_insert)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(event_callback_msg_id_insert) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(event_callback_msg_id_insert): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);
    
    /* HERE is a SPECIAL case: the callback function inserted is "the proxy one" */
    /* event_callback_msg_id_insert does not return err-code currently (return void) */
    event_callback_msg_id_insert(payload.event_type, 
                                 payload.name, 
                                 payload.priority, 
                                 payload.msg_id, 
                                 callback_wrapper_fp_arr[payload.event_type] );

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;
    ack_packet.ret_val = EA_ERR_OK; /*currently no other err-code for this api */
    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(event_callback_msg_id_insert): " 
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api*/
    return ret;
}


/* com_packet_processing.h */
int WRAPPER_FUNC_OF(cloud_packet_tx)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(cloud_packet_tx) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(cloud_packet_tx): " 
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);


    unsigned char specific_field[ payload.len ];
    ret = read_from_unix_socket_fd(client_fd, &specific_field, sizeof(payload.len));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(cloud_packet_tx): " 
                        "get specific_field from client_fd:%d, ret = %d\n", client_fd, ret);

    /* call the actual function */
    /* cloud_packet_tx does not return err-code currently (return void) */
    cloud_packet_tx(payload.len, payload.service_id, specific_field);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;
    ack_packet.ret_val = EA_ERR_OK; /*currently no other err-code for this api */
    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(cloud_packet_tx): "  
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api*/
    return ret;
}

int WRAPPER_FUNC_OF(OBU_j2735_tx)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(OBU_j2735_tx) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_j2735_tx): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);


    uint8_t buf[ payload.buf_len ];
    ret = read_from_unix_socket_fd(client_fd, buf, sizeof(payload.buf_len));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_j2735_tx): "
                        "get buf from client_fd:%d, ret = %d\n", client_fd, ret);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side 
     * we direcly use com_send() */
    ret = com_send(GENERAL_COM_ID, buf, payload.buf_len);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("WRAPPER_FUNC_OF(OBU_j2735_tx): com_send");
        ack_packet.ret_val = EA_ERR_COM_IO;
    }
    else{
        ack_packet.ret_val = EA_ERR_OK;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_j2735_tx): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}

int WRAPPER_FUNC_OF(OBU_packet_tx)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(OBU_packet_tx) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_packet_tx): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);


    uint8_t write_buf_content[ payload.write_buf_len ];
    ret = read_from_unix_socket_fd(client_fd, write_buf_content, sizeof(payload.write_buf_len));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_packet_tx): "
                        "get write_buf_content from client_fd:%d, ret = %d\n", client_fd, ret);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side 
     * we direcly use com_send() */
    ret = com_send(GENERAL_COM_ID, write_buf_content, payload.write_buf_len);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("WRAPPER_FUNC_OF(OBU_packet_tx): com_send");
        ack_packet.ret_val = EA_ERR_COM_IO;
    }
    else{
        ack_packet.ret_val = EA_ERR_OK;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(OBU_packet_tx): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}

int WRAPPER_FUNC_OF(remote_com_send)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(remote_com_send) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(remote_com_send): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);


    uint8_t buf[ payload.buf_len ];
    ret = read_from_unix_socket_fd(client_fd, buf, sizeof(payload.buf_len));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(remote_com_send): "
                        "get buf from client_fd:%d, ret = %d\n", client_fd, ret);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side 
     * we direcly use com_send() */
    ret = com_send(GENERAL_COM_ID, buf, payload.buf_len);
    if (ret == COM_IO_ERR) {
        log_file_write_fatal_error("WRAPPER_FUNC_OF(remote_com_send): com_send");
        ack_packet.ret_val = EA_ERR_COM_IO;
    }
    else{
        ack_packet.ret_val = EA_ERR_OK;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(remote_com_send): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}


/* config.h */
int WRAPPER_FUNC_OF(get_config_RSU_id)(int client_fd)
{
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    // int ret;
    // //no req_payload from this api

    // ack_from_proxy_header_t ack_packet;
    // ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    // struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    // ack_payload.RSU_id = config.RSU_id;
    // ack_packet.ret_val = EA_ERR_OK;

    // ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    // if (ret != 0) {
    //     ;//maybe log err
    //     return ret;
    // }

    // ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    // if (ret != 0) {
    //     ;//maybe log err
    // }
    // return ret;
}

int WRAPPER_FUNC_OF(get_config_RSU_lat)(int client_fd)
{   
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    // int ret;
    // //no req_payload from this api

    // ack_from_proxy_header_t ack_packet;
    // ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    // struct ACK_PAYLOAD_TYPE(get_config_RSU_lat) ack_payload;
    // ack_payload.RSU_lat = config.RSU_lat;
    // ack_packet.ret_val = EA_ERR_OK;

    // ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    // if (ret != 0) {
    //     ;//maybe log err
    //     return ret;
    // }

    // ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    // if (ret != 0) {
    //     ;//maybe log err
    // }
    // return ret;
}

int WRAPPER_FUNC_OF(get_config_RSU_lon)(int client_fd)
{
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    int ret;
    //no req_payload from this api

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_lon) ack_payload;
    ack_payload.RSU_lon = config.RSU_lon;
    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_config_RSU_name)(int client_fd)
{
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    // int ret;
    // //no req_payload from this api

    // ack_from_proxy_header_t ack_packet;
    // ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    // struct ACK_PAYLOAD_TYPE(get_config_RSU_name) ack_payload;
    // strncpy(ack_payload.RSU_name_arr, config.RSU_name, RSU_NAME_MAX_LEN ); 
    // ack_packet.ret_val = EA_ERR_OK;

    // ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    // if (ret != 0) {
    //     ;//maybe log err
    //     return ret;
    // }
    // ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    // if (ret != 0) {
    //     ;//maybe log err
    // }
    // return ret;
}

int WRAPPER_FUNC_OF(get_config_RSU_region)(int client_fd)
{
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    int ret;
    //no req_payload from this api

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_region) ack_payload;
    ack_payload.RSU_region = config.RSU_region;
    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_config_RSU_elev)(int client_fd)
{
    /* current version will update remote RSU config when registered, 
       so this WRAPPER_FUNC should never be called */
    return 0;

    // int ret;
    // //no req_payload from this api

    // ack_from_proxy_header_t ack_packet;
    // ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    // struct ACK_PAYLOAD_TYPE(get_config_RSU_elev) ack_payload;
    // ack_payload.RSU_elev = config.RSU_elev;
    // ack_packet.ret_val = EA_ERR_OK;

    // ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    // if (ret != 0) {
    //     ;//maybe log err
    //     return ret;
    // }

    // ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    // if (ret != 0) {
    //     ;//maybe log err
    // }
    // return ret;
}


/* traffic_signal_command_buffer.h */
int WRAPPER_FUNC_OF(command_buf_insert_effect_time)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(command_buf_insert_effect_time) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(command_buf_insert_effect_time): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    /* call the actual function */
    ret = command_buf_insert_effect_time( &payload.tsc_cmd );
    if (ret != 0) {
        log_file_write("WRAPPER_FUNC_OF(command_buf_insert_effect_time)");
        ack_packet.ret_val = EA_ERR_ACTUAL_FUNCTION_ERR;
    }
    else{
        ack_packet.ret_val = EA_ERR_OK;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(command_buf_insert_effect_time): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}

int WRAPPER_FUNC_OF(command_buf_insert_adjustment)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(command_buf_insert_adjustment) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(command_buf_insert_adjustment): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    /* call the actual function */
    ret = command_buf_insert_adjustment( &payload.tsc_cmd );
    if (ret != 0) {
        log_file_write("WRAPPER_FUNC_OF(command_buf_insert_adjustment)");
        ack_packet.ret_val = EA_ERR_ACTUAL_FUNCTION_ERR;
    }
    else{
        ack_packet.ret_val = EA_ERR_OK;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(command_buf_insert_adjustment): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}


/* vms.h */
int WRAPPER_FUNC_OF(vms_request_start)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(vms_request_start) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_request_start): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);

    /* call the actual function */
    vms_request_start(payload.id, payload.priority);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;
    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_request_start): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}

int WRAPPER_FUNC_OF(vms_request_end)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(vms_request_end) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_request_end): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);

    /* call the actual function */
    vms_request_end(payload.id);

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;
    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_request_end): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}

int WRAPPER_FUNC_OF(vms_sync_evsp_prog)(int client_fd)
{
    int ret;
    struct REQ_PAYLOAD_TYPE(vms_sync_evsp_prog) payload;   
    ret = read_from_unix_socket_fd(client_fd, &payload, sizeof(payload));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_sync_evsp_prog): "
                        "get payload from client_fd:%d, ret = %d\n", client_fd, ret);

    /* do the actual update */
    for(size_t i = 0; i < RTM_MAX; ++i){
        evsp_prog[i] = payload.evsp_prog[i];
    }

    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;
    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if(PRINT_MSG_FOR_DEBUG)
        fprintf(stdout, "WRAPPER_FUNC_OF(vms_sync_evsp_prog): "
                        "send ack_packet to client_fd:%d, ret = %d\n", client_fd, ret);
    if (ret != 0) {
        ;//maybe log err
    }
    
    /* no ack_payload for this api */
    return ret;
}



/* traffic_signal_status_updating.h */
int WRAPPER_FUNC_OF(get_traffic_signal_status)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_current_traffic_signal_status)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_current_phase)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_current_step)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_current_second)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_SubPhaseCount)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_SignalCount)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_plan_id)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_control_status)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_PhaseOrder)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_remaining_time)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_SignalStatus)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_total_compensation_second)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_compensation_buffer)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(set_control_status)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_original_tc_health_status)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_next_SubPhaseID)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}

int WRAPPER_FUNC_OF(get_prev_SubPhaseID)(int client_fd)
{
    int ret;
    //struct REQ_PAYLOAD_TYPE(get_config_RSU_id) payload;   //no payload from this api
    ack_from_proxy_header_t ack_packet;
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    struct ACK_PAYLOAD_TYPE(get_config_RSU_id) ack_payload;
    ack_payload.RSU_id = config.RSU_id;

    ack_packet.ret_val = EA_ERR_OK;

    ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    if (ret != 0) {
        ;//maybe log err
        return ret;
    }

    ret = send_to_unix_socket_fd( client_fd, &ack_payload, sizeof(ack_payload));
    if (ret != 0) {
        ;//maybe log err
    }
    return ret;
}



eap_api_wrapper_fp api_wrapper_fp_arr[NUM_OF_API_ID_DEFININITION] = {
    /* application_registration.h */
    [API_ID_OF(event_callback_msg_id_insert)] = WRAPPER_FUNC_OF(event_callback_msg_id_insert),

    /* com_packet_processing.h */
    [API_ID_OF(cloud_packet_tx)] = WRAPPER_FUNC_OF(cloud_packet_tx),
    [API_ID_OF(OBU_j2735_tx)] = WRAPPER_FUNC_OF(OBU_j2735_tx),
    [API_ID_OF(OBU_packet_tx)] = WRAPPER_FUNC_OF(OBU_packet_tx),
    [API_ID_OF(remote_com_send)] = WRAPPER_FUNC_OF(remote_com_send),

    /* config.h */
    [API_ID_OF(get_config_RSU_id)] = WRAPPER_FUNC_OF(get_config_RSU_id),
    [API_ID_OF(get_config_RSU_lat)] = WRAPPER_FUNC_OF(get_config_RSU_lat),
    [API_ID_OF(get_config_RSU_lon)] = WRAPPER_FUNC_OF(get_config_RSU_lon),
    [API_ID_OF(get_config_RSU_name)] = WRAPPER_FUNC_OF(get_config_RSU_name),
    [API_ID_OF(get_config_RSU_region)] = WRAPPER_FUNC_OF(get_config_RSU_region),
    [API_ID_OF(get_config_RSU_elev)] = WRAPPER_FUNC_OF(get_config_RSU_elev),

    /* traffic_signal_command_buffer.h */
    [API_ID_OF(command_buf_insert_effect_time)] = WRAPPER_FUNC_OF(command_buf_insert_effect_time),
    [API_ID_OF(command_buf_insert_effect_time)] = WRAPPER_FUNC_OF(command_buf_insert_effect_time),

    /* vms.h */    
    [API_ID_OF(vms_request_start)] = WRAPPER_FUNC_OF(vms_request_start),
    [API_ID_OF(vms_request_end)] = WRAPPER_FUNC_OF(vms_request_end),
    [API_ID_OF(vms_sync_evsp_prog)] = WRAPPER_FUNC_OF(vms_sync_evsp_prog),

    /* traffic_signal_status_updating.h */
    [API_ID_OF(get_traffic_signal_status)] = WRAPPER_FUNC_OF(get_traffic_signal_status),
    [API_ID_OF(get_current_traffic_signal_status)] = WRAPPER_FUNC_OF(get_current_traffic_signal_status),
    [API_ID_OF(get_current_phase)] = WRAPPER_FUNC_OF(get_current_phase),
    [API_ID_OF(get_current_step)] = WRAPPER_FUNC_OF(get_current_step),
    [API_ID_OF(get_current_second)] = WRAPPER_FUNC_OF(get_current_second),
    [API_ID_OF(get_SubPhaseCount)] = WRAPPER_FUNC_OF(get_SubPhaseCount),
    [API_ID_OF(get_SignalCount)] = WRAPPER_FUNC_OF(get_SignalCount),
    [API_ID_OF(get_plan_id)] = WRAPPER_FUNC_OF(get_plan_id),
    [API_ID_OF(get_control_status)] = WRAPPER_FUNC_OF(get_control_status),
    [API_ID_OF(get_PhaseOrder)] = WRAPPER_FUNC_OF(get_PhaseOrder),
    [API_ID_OF(get_remaining_time)] = WRAPPER_FUNC_OF(get_remaining_time),
    [API_ID_OF(get_SignalStatus)] = WRAPPER_FUNC_OF(get_SignalStatus),
    [API_ID_OF(get_total_compensation_second)] = WRAPPER_FUNC_OF(get_total_compensation_second),
    [API_ID_OF(get_compensation_buffer)] = WRAPPER_FUNC_OF(get_compensation_buffer),
    [API_ID_OF(set_control_status)] = WRAPPER_FUNC_OF(set_control_status),
    [API_ID_OF(get_original_tc_health_status)] = WRAPPER_FUNC_OF(get_original_tc_health_status),
    [API_ID_OF(get_next_SubPhaseID)] = WRAPPER_FUNC_OF(get_next_SubPhaseID),
    [API_ID_OF(get_prev_SubPhaseID)] = WRAPPER_FUNC_OF(get_prev_SubPhaseID),
};



