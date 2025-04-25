#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "application_registration.h"
#include "com_io.h"
#include "com_packet_processing.h"
#include "config.h"
#include "log.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
#include "vms.h"
// #include "j2735inc/j2735_codec.h"

#include "external_app_proxy_api_wrapper.h"
#include "external_app_proxy_callback_msg_forward.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_socket.h"
#include "external_app_proxy_typedefine.h"

LOG_USE_MODULE(MIDDLEWARE_EXTENAL_APP_PROXY);

static inline __attribute__((always_inline)) int simple_send_ack_to_app(int fd, int ack_ret_val)
{
    int ret;
    packet_header_from_proxy_t ack;
    ack.packet_type = EA_PACKET_TYPE_ACK;
    ack.ret_val = ack_ret_val;
    ret = eap_send_packet_to_unix_sk(fd, &ack, sizeof(ack));

    LOG_MSG_INFO("[EAP msg] for the request, ack.ret_val is:%d ->%s",
            ack_ret_val, get_str_by_err_code(ack_ret_val));

    return ret;
}

static inline __attribute__((always_inline)) int simple_recv_packet_from_app(int client_fd, void *packet_p, size_t packet_size, char *log_msg)
{
    int ret = eap_recv_packet_from_unix_sk(client_fd, packet_p, packet_size);
    if (ret) {
        LOG_MSG_INFO("[EAP MSG] %s: get %s from client_fd:%d, ret = %d",
                __func__, log_msg, client_fd, ret);
    }
    return ret;
}

static inline __attribute__((always_inline)) int simple_sent_packet_to_app(int client_fd, void *packet_p, size_t packet_size, char *log_msg)
{
    int ret = eap_send_packet_to_unix_sk(client_fd, packet_p, packet_size);
    if (ret) {
        LOG_MSG_INFO("[EAP MSG] %s: get %s from client_fd:%d, ret = %d",
                __func__, log_msg, client_fd, ret);
    }
    return ret;
}

#define API_WRAPPER_OF(api_name) api_name##_api_wrapper

/* application_registration.h */
int API_WRAPPER_OF(event_callback_msg_id_insert)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in event_callback_msg_id_insert() in ea_library
    */
    struct {
        event_type_t event_type;
        char name[APP_NAME_MAX_LEN];
        int priority;
        DSRCmsgID msg_id;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* HERE is a SPECIAL case: the callback function inserted is "the proxy one" */
    /* event_callback_msg_id_insert does not return err-code currently (return void) */
    event_callback_msg_id_insert(payload.event_type,
                                 payload.name,
                                 payload.priority,
                                 payload.msg_id,
                                 cbmsg_forward_fp_arr[payload.event_type]);

    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);

    /* no ack_payload for this api*/
    return ret;
}

/* com_packet_processing.h */
int API_WRAPPER_OF(cloud_packet_tx)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in cloud_packet_tx_needAck() in ea_library
    */
    struct {
        uint16_t len;
        uint8_t service_id;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    unsigned char specific_field[payload.len];
    ret = simple_recv_packet_from_app(client_fd, specific_field, sizeof(payload.len), "specific_field");
    if (ret)
        return ret;

    // ret = eap_recv_packet_from_unix_sk(client_fd, specific_field, sizeof(payload.len));
    // if( ret ){
    //     LOG_MSG_INFO("[EAP MSG] %s: get specific_field from client_fd:%d, ret = %d",
    //             __func__, client_fd, ret);
    //     return ret;
    // }

    /* call the actual function */
    /* cloud_packet_tx does not return err-code currently (return void) */
    cloud_packet_tx(payload.len, payload.service_id, specific_field);

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(OBU_j2735_tx)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in OBU_j2735_tx() in ea_library
    */
    struct {
        size_t buf_len;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    uint8_t buf[payload.buf_len];
    ret = simple_recv_packet_from_app(client_fd, buf, sizeof(payload.buf_len), "buf");
    if (ret)
        return ret;

    // ret = eap_recv_packet_from_unix_sk(client_fd, buf, sizeof(payload.buf_len));
    // if( ret ){
    //     LOG_MSG_INFO("[EAP MSG] %s: get buf from client_fd:%d, ret = %d",
    //             __func__, client_fd, ret);
    //     return ret;
    // }

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side
     * we direcly use com_send() */
    int ack_ret_val;
    ret = com_send(GENERAL_COM_ID, buf, payload.buf_len);
    if (ret == COM_IO_ERR) {
        ack_ret_val = -EAL_ERR_IN_MIDDLEWARE_ERR_COM_IO;
        LOG_MSG_INFO("[EAP MSG] err: %s: com_send() ret:%d", __func__, ret);
    } else {
        ack_ret_val = EAL_ERR_OK;
    }

    ret = simple_send_ack_to_app(client_fd, ack_ret_val);

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(OBU_packet_tx)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is recv in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "recv" and "send" pairs in OBU_packet_tx() in ea_library
    */
    struct {
        size_t write_buf_len;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    uint8_t write_buf[payload.write_buf_len];
    ret = simple_recv_packet_from_app(client_fd, write_buf,
                                      sizeof(payload.write_buf_len), "payload.write_buf_len");
    if (ret)
        return ret;

    // ret = eap_recv_packet_from_unix_sk(client_fd, write_buf, sizeof(payload.write_buf_len));
    // if( ret ){
    //     LOG_MSG_INFO("[EAP MSG] %s: get write_buf from client_fd:%d, ret = %d",
    //             __func__, client_fd, ret);
    //     return ret;
    // }

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side
     * we direcly use com_send() */
    int ack_ret_val;
    ret = com_send(GENERAL_COM_ID, write_buf, payload.write_buf_len);
    if (ret == COM_IO_ERR) {
        ack_ret_val = -EAL_ERR_IN_MIDDLEWARE_ERR_COM_IO;
        LOG_MSG_INFO("[EAP MSG] err: %s: com_send() ret:%d", __func__, ret);
    } else {
        ack_ret_val = EAL_ERR_OK;
    }

    ret = simple_send_ack_to_app(client_fd, ack_ret_val);

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(remote_com_send)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in remote_com_send() in ea_library
    */
    struct {
        size_t buf_len;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    uint8_t buf[payload.buf_len];
    ret = simple_recv_packet_from_app(client_fd, buf, sizeof(payload.buf_len), "payload.buf_len");
    if (ret)
        return ret;

    // ret = eap_recv_packet_from_unix_sk(client_fd, buf, sizeof(payload.buf_len));
    // if( ret ){
    //     LOG_MSG_INFO("[EAP MSG] %s: get buf from client_fd:%d, ret = %d",
    //             __func__, client_fd, ret);
    //     return ret;
    // }

    /* call the actual function */
    /* HERE is a SPECIAL case: since the data is already pre-processed at client side
     * we direcly use com_send() */
    int ack_ret_val;
    ret = com_send(GENERAL_COM_ID, buf, payload.buf_len);
    if (ret == COM_IO_ERR) {
        ack_ret_val = -EAL_ERR_IN_MIDDLEWARE_ERR_COM_IO;
        LOG_MSG_INFO("[EAP MSG] err: %s: com_send() ret:%d", __func__, ret);
    } else {
        ack_ret_val = EAL_ERR_OK;
    }

    ret = simple_send_ack_to_app(client_fd, ack_ret_val);

    /* no ack_payload for this api*/
    return ret;
}

/* config.h */
int API_WRAPPER_OF(get_config_RSU_id)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}
int API_WRAPPER_OF(get_config_RSU_lat)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}
int API_WRAPPER_OF(get_config_RSU_lon)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}
int API_WRAPPER_OF(get_config_RSU_name)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}
int API_WRAPPER_OF(get_config_RSU_region)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}
int API_WRAPPER_OF(get_config_RSU_elev)(int client_fd)
{
    /* current version will update remote RSU config when registered,
       so this WRAPPER_FUNC should never be called */
    return 0;
}

/* traffic_signal_command_buffer.h */
int API_WRAPPER_OF(command_buf_insert_effect_time)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in ea_library
    */
    struct {
        tsc_command_t tsc_cmd;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* call the actual function */
    int ack_ret_val;
    ret = command_buf_insert_effect_time(&payload.tsc_cmd);
    if (ret) {
        ack_ret_val = -EAL_ERR_IN_MIDDLEWARE_API_INTERNAL;
        LOG_MSG_INFO("[EAP MSG] %s: command_buf_insert_effect_time ret :%d", __func__, ret);
    } else {
        ack_ret_val = EAL_ERR_OK;
    }

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, ack_ret_val);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(command_buf_insert_adjustment)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in ea_library
    */
    struct {
        tsc_command_t tsc_cmd;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* call the actual function */
    int ack_ret_val;
    ret = command_buf_insert_adjustment(&payload.tsc_cmd);
    if (ret) {
        ack_ret_val = -EAL_ERR_IN_MIDDLEWARE_API_INTERNAL;
        LOG_MSG_INFO("[EAP MSG] %s: command_buf_insert_adjustment ret :%d", __func__, ret);
    } else {
        ack_ret_val = EAL_ERR_OK;
    }

    ret = simple_send_ack_to_app(client_fd, ack_ret_val);

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(command_buf_delete_OBU)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in ea_library
    */
    struct {
        char OBU_name[ID_MAX_LEN + 1];
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    char host_OBU_name[ID_MAX_LEN + 1];
    strncpy(host_OBU_name, payload.OBU_name, ID_MAX_LEN + 1);

    /* call the actual function */
    command_buf_delete_OBU(host_OBU_name);

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    int ack_ret_val = EAL_ERR_OK;
    ret = simple_send_ack_to_app(client_fd, ack_ret_val);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(command_buf_resume_control)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in ea_library
    */
    struct {
        uint8_t appid;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* call the actual function */
    int ack_ret_val;
    ret = command_buf_resume_control(payload.appid);
    if(ret){
        ack_ret_val = EAL_ERR_IN_MIDDLEWARE_COMMAND_BUF_RESUME_CTL;
    }
    else{
        ack_ret_val = EAL_ERR_OK;
    }

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, ack_ret_val);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(command_buf_search)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in command_buf_search in ea_library
    */
    struct {
        int cycle;
        int subphaseID;
    } payload;

    int ret = eap_recv_packet_from_unix_sk(client_fd, &payload, sizeof(payload));
    LOG_MSG_DEBUG("%s: get payload from client_fd:%d, ret = %d", __func__, client_fd, ret);
    if (ret) {
        return ret;
    }

    struct {
        tsc_command_object_t command_obj;
    } ack_payload;

    /* call the actual function */
    command_buf_search(payload.cycle, payload.subphaseID, &(ack_payload.command_obj) );

    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}

/* vms.h */
int API_WRAPPER_OF(vms_request_start)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in vms_request_start() in ea_library
    */
    struct {
        uint8_t id;
        uint8_t priority;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* call the actual function */
    vms_request_start(payload.id, payload.priority);

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
#endif

    /* no ack and no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(vms_request_end)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in event_callback_msg_id_insert() in ea_library
    */
    struct {
        uint8_t id;
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* call the actual function */
    vms_request_end(payload.id);

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(vms_sync_evsp_prog)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in vms_sync_evsp_prog() in ea_library
    */
    struct {
        uint8_t evsp_prog[RTM_MAX];
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* do the actual update */
    for (size_t i = 0; i < RTM_MAX; ++i) {
        evsp_prog[i] = payload.evsp_prog[i];
    }

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
#endif

    /* no ack_payload for this api*/
    return ret;
}
int API_WRAPPER_OF(vms_sync_then_start)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in vms_sync_then_start_needAck() in ea_library
    */
    struct {
        uint8_t id;
        uint8_t priority;
        uint8_t evsp_prog[RTM_MAX];
    } payload;

    int ret = simple_recv_packet_from_app(client_fd, &payload, sizeof(payload), "payload");
    if (ret)
        return ret;

    /* do the actual update */
    for (size_t i = 0; i < RTM_MAX; ++i) {
        evsp_prog[i] = payload.evsp_prog[i];
    }
    /* then call the actual function */
    vms_request_start(payload.id, payload.priority);

#if USING_NON_ACK_CONFIG
    ;  // do not send ack
#else
    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
#endif

    /* no ack_payload for this api*/
    return ret;
}

/* traffic_signal_status_updating.h */
int API_WRAPPER_OF(get_traffic_signal_status)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_traffic_signal_status() in ea_library
    */
    struct {
        traffic_signal_status_t ts_status;
    } ack_payload;

    /* call the actual function */
    get_traffic_signal_status(&(ack_payload.ts_status));

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_current_traffic_signal_status)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_current_traffic_signal_status() in ea_library
    */
    struct {
        traffic_signal_status_t cur_ts_status;
    } ack_payload;

    /* call the actual function */
    get_current_traffic_signal_status(&(ack_payload.cur_ts_status));

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_current_phase)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_current_phase() in ea_library
    */
    struct {
        uint8_t phase;
    } ack_payload;

    /* call the actual function */
    ack_payload.phase = get_current_phase();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_current_step)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_current_step() in ea_library
    */
    struct {
        uint8_t step;
    } ack_payload;

    /* call the actual function */
    ack_payload.step = get_current_step();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_current_second)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_current_second() in ea_library
    */
    struct {
        uint16_t second;
    } ack_payload;

    /* call the actual function */
    ack_payload.second = get_current_second();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_SubPhaseCount)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_current_second() in ea_library
    */
    struct {
        uint8_t SubPhaseCount;
    } ack_payload;

    /* call the actual function */
    ack_payload.SubPhaseCount = get_SubPhaseCount();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_SignalCount)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_SignalCount() in ea_library
    */
    struct {
        uint8_t SignalCount;
    } ack_payload;

    /* call the actual function */
    ack_payload.SignalCount = get_SignalCount();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_plan_id)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_plan_id in ea_library
    */
    struct {
        uint8_t plan_id;
    } ack_payload;

    /* call the actual function */
    ack_payload.plan_id = get_plan_id();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_control_status)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_control_status in ea_library
    */
    struct {
        uint8_t ctrl_status;
    } ack_payload;

    /* call the actual function */
    ack_payload.ctrl_status = get_control_status();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_PhaseOrder)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_PhaseOrder in ea_library
    */
    struct {
        uint8_t PhaseOrder;
    } ack_payload;

    /* call the actual function */
    ack_payload.PhaseOrder = get_PhaseOrder();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_remaining_time)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_remaining_time in ea_library
    */
    struct {
        uint8_t phase;
        uint8_t step;
        uint16_t second;
    } payload;

    int ret = eap_recv_packet_from_unix_sk(client_fd, &payload, sizeof(payload));
    LOG_MSG_DEBUG("%s: get payload from client_fd:%d, ret = %d", __func__, client_fd, ret);
    if (ret) {
        return ret;
    }

    struct {
        uint8_t remaining_time;
    } ack_payload;

    /* call the actual function */
    ack_payload.remaining_time = get_remaining_time(payload.phase, payload.step, payload.second);

    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_SignalStatus)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_SignalStatus in ea_library
    */
    struct {
        uint8_t SubPhaseCount_index;
        uint8_t SignalCount_index;
    } payload;

    int ret = eap_recv_packet_from_unix_sk(client_fd, &payload, sizeof(payload));
    LOG_MSG_DEBUG("%s: get payload from client_fd:%d, ret = %d\n", __func__, client_fd, ret);
    if (ret) {
        return ret;
    }

    struct {
        uint8_t SignalStatus;
    } ack_payload;

    /* call the actual function */
    ack_payload.SignalStatus = get_SignalStatus(payload.SubPhaseCount_index, payload.SignalCount_index);

    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_total_compensation_second)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_total_compensation_second in ea_library
    */
    struct {
        int16_t total_cps_sec;
    } ack_payload;

    /* call the actual function */
    ack_payload.total_cps_sec = get_total_compensation_second();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(set_control_status)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in set_control_status in ea_library
    */
    struct {
        uint8_t control_status;
    } payload;

    int ret = eap_recv_packet_from_unix_sk(client_fd, &payload, sizeof(payload));
    LOG_MSG_DEBUG("%s: get payload from client_fd:%d, ret = %d", __func__, client_fd, ret);
    if (ret) {
        return ret;
    }

    /* call the actual function */
    set_control_status(payload.control_status);

    ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    return ret;
}
int API_WRAPPER_OF(get_original_tc_health_status)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_original_tc_health_status in ea_library
    */
    struct {
        uint16_t ori_tc_health_status;
    } ack_payload;

    /* call the actual function */
    ack_payload.ori_tc_health_status = get_original_tc_health_status();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_next_SubPhaseID)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_next_SubPhaseID in ea_library
    */
    struct {
        uint8_t next_SubPhaseID;
    } ack_payload;

    /* call the actual function */
    ack_payload.next_SubPhaseID = get_next_SubPhaseID();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}
int API_WRAPPER_OF(get_prev_SubPhaseID)(int client_fd)
{
    /* WARNNING!!!
       except for packet_header_to_proxy_t, which is read in handle_registered_client_packet(),
       make sure the "payload" you "recv" and "ack_payload" you "send",
       "MATCH" the "read" and "send" pairs in get_next_SubPhaseID in ea_library
    */
    struct {
        uint8_t prev_SubPhaseID;
    } ack_payload;

    /* call the actual function */
    ack_payload.prev_SubPhaseID = get_prev_SubPhaseID();

    int ret = simple_send_ack_to_app(client_fd, EAL_ERR_OK);
    if (ret) {
        return ret;
    }
    ret = simple_sent_packet_to_app(client_fd, &ack_payload, sizeof(ack_payload), "ack_payload");
    return ret;
}

eap_api_wrapper_fp api_wrapper_fp_arr[] = {
    /* application_registration.h */
    [API_ID_OF(event_callback_msg_id_insert)] = API_WRAPPER_OF(event_callback_msg_id_insert),

    /* com_packet_processing.h */
    [API_ID_OF(cloud_packet_tx)] = API_WRAPPER_OF(cloud_packet_tx),
    [API_ID_OF(OBU_j2735_tx)] = API_WRAPPER_OF(OBU_j2735_tx),
    [API_ID_OF(OBU_packet_tx)] = API_WRAPPER_OF(OBU_packet_tx),
    [API_ID_OF(remote_com_send)] = API_WRAPPER_OF(remote_com_send),

    /* config.h */
    [API_ID_OF(get_config_RSU_id)] = API_WRAPPER_OF(get_config_RSU_id),
    [API_ID_OF(get_config_RSU_lat)] = API_WRAPPER_OF(get_config_RSU_lat),
    [API_ID_OF(get_config_RSU_lon)] = API_WRAPPER_OF(get_config_RSU_lon),
    [API_ID_OF(get_config_RSU_name)] = API_WRAPPER_OF(get_config_RSU_name),
    [API_ID_OF(get_config_RSU_region)] = API_WRAPPER_OF(get_config_RSU_region),
    [API_ID_OF(get_config_RSU_elev)] = API_WRAPPER_OF(get_config_RSU_elev),

    /* traffic_signal_command_buffer.h */
    [API_ID_OF(command_buf_insert_effect_time)] = API_WRAPPER_OF(command_buf_insert_effect_time),
    [API_ID_OF(command_buf_insert_adjustment)] = API_WRAPPER_OF(command_buf_insert_adjustment),
    [API_ID_OF(command_buf_delete_OBU)] = API_WRAPPER_OF(command_buf_delete_OBU),
    [API_ID_OF(command_buf_resume_control)] = API_WRAPPER_OF(command_buf_resume_control),
    [API_ID_OF(command_buf_search)] = API_WRAPPER_OF(command_buf_search),

    /* vms.h */
    [API_ID_OF(vms_request_start)] = API_WRAPPER_OF(vms_request_start),
    [API_ID_OF(vms_request_end)] = API_WRAPPER_OF(vms_request_end),
    [API_ID_OF(vms_sync_evsp_prog)] = API_WRAPPER_OF(vms_sync_evsp_prog),
    [API_ID_OF(vms_sync_then_start)] = API_WRAPPER_OF(vms_sync_then_start),

    /* traffic_signal_status_updating.h */
    [API_ID_OF(get_traffic_signal_status)] = API_WRAPPER_OF(get_traffic_signal_status),
    [API_ID_OF(get_current_traffic_signal_status)] = API_WRAPPER_OF(get_current_traffic_signal_status),
    [API_ID_OF(get_current_phase)] = API_WRAPPER_OF(get_current_phase),
    [API_ID_OF(get_current_step)] = API_WRAPPER_OF(get_current_step),
    [API_ID_OF(get_current_second)] = API_WRAPPER_OF(get_current_second),
    [API_ID_OF(get_SubPhaseCount)] = API_WRAPPER_OF(get_SubPhaseCount),
    [API_ID_OF(get_SignalCount)] = API_WRAPPER_OF(get_SignalCount),
    [API_ID_OF(get_plan_id)] = API_WRAPPER_OF(get_plan_id),
    [API_ID_OF(get_control_status)] = API_WRAPPER_OF(get_control_status),
    [API_ID_OF(get_PhaseOrder)] = API_WRAPPER_OF(get_PhaseOrder),
    [API_ID_OF(get_remaining_time)] = API_WRAPPER_OF(get_remaining_time),
    [API_ID_OF(get_SignalStatus)] = API_WRAPPER_OF(get_SignalStatus),
    [API_ID_OF(get_total_compensation_second)] = API_WRAPPER_OF(get_total_compensation_second),
    [API_ID_OF(set_control_status)] = API_WRAPPER_OF(set_control_status),
    [API_ID_OF(get_original_tc_health_status)] = API_WRAPPER_OF(get_original_tc_health_status),
    [API_ID_OF(get_next_SubPhaseID)] = API_WRAPPER_OF(get_next_SubPhaseID),
    [API_ID_OF(get_prev_SubPhaseID)] = API_WRAPPER_OF(get_prev_SubPhaseID),
};
