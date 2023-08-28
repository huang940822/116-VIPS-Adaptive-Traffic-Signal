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

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_api_wrapper.h"

#define WRAPPER_FUNC_OF(api_name) api_name ## _wrapper_func

/* application_registration.h */
int WRAPPER_FUNC_OF(event_callback_msg_id_insert)(int client_fd)
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


/* com_packet_processing.h */
int WRAPPER_FUNC_OF(cloud_packet_tx)(int client_fd)
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

int WRAPPER_FUNC_OF(remote_com_send_OBU)(int client_fd)
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


/* config.h */
int WRAPPER_FUNC_OF(get_config_RSU_id)(int client_fd)
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

int WRAPPER_FUNC_OF(get_config_RSU_lat)(int client_fd)
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

int WRAPPER_FUNC_OF(get_config_RSU_lon)(int client_fd)
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

int WRAPPER_FUNC_OF(get_config_RSU_name)(int client_fd)
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

int WRAPPER_FUNC_OF(get_config_RSU_region)(int client_fd)
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

int WRAPPER_FUNC_OF(get_config_RSU_elev)(int client_fd)
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


/* traffic_signal_command_buffer.h */
int WRAPPER_FUNC_OF(command_buf_insert_effect_time)(int client_fd)
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

int WRAPPER_FUNC_OF(command_buf_insert_adjustment)(int client_fd)
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


/* vms.h */
int WRAPPER_FUNC_OF(vms_request_start)(int client_fd)
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

int WRAPPER_FUNC_OF(vms_request_end)(int client_fd)
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



eap_api_wrapper_fp wrapper_fp_arr[NUM_OF_API_ID_DEFININITION]{
    /* application_registration.h */
    [API_ID_OF(event_callback_msg_id_insert)] = WRAPPER_FUNC_OF(event_callback_msg_id_insert),

    /* com_packet_processing.h */
    [API_ID_OF(cloud_packet_tx)] = WRAPPER_FUNC_OF(cloud_packet_tx),
    [API_ID_OF(remote_com_send_OBU)] = WRAPPER_FUNC_OF(remote_com_send_OBU),

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



