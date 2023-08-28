#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/epoll.h> // for epoll_create1()

#include "log.h"
#include "typedefine.h"
#include "application_registration.h"
#include "application_helper.h"
#include "config.h"

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_api_wrapper.h"

#define WRAPPER_FUNC_OF(api_name) api_name ## _wrapper_func

// a lot to do
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


eap_api_wrapper_fp wrapper_fp_arr[NUM_OF_API_ID_DEFININITION]{
    [API_ID_OF(get_config_RSU_id)] = WRAPPER_FUNC_OF(get_config_RSU_id),
};



