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
#include "ObstacleList.h"
#include "j2735inc/j2735_codec.h"

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_callback_wrapper.h"

/* NOTICE, if you add new callback, you NEED to add a new EAP_CALLBACK_WRAPPER */
/* and update the callback_wrapper_fp_arr[]  */

app_obj_t* proxy_handling_app_p;

static inline int eap_callback_wrapper_checker(void *app_section, char* func_name)
{
    if(!app_section){
        fprintf(stderr,"%s: app_section be assigned NULL ptr!\n", func_name);
        log_file_write_fatal_error("%s: app_section be assigned NULL ptr!", func_name);
        return -1;
    }
    if(!proxy_handling_app_p){
        fprintf(stderr,"%s: proxy_handling_app_p be assigned NULL ptr!\n", func_name);
        log_file_write_fatal_error("%s: proxy_handling_app_p be assigned NULL ptr!", func_name);
        return -1;
    }
    if( proxy_handling_app_p->ea_info_p == 0){
        fprintf(stderr,"%s: be called when the app is not external\n", func_name);
        log_file_write_fatal_error("%s: be called when the app is not external\n", func_name);
        return -1;
    }
    return 0;
}

/* below are all the wrapper functions for all event callback */
/* NOTICE, if you add new event callback, you NEED to add a related wrapper function */
#define EAP_CALLBACK_WRAPPER_OF(event_name) event_name ## _callback_wrapper

int EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_rx)(void *app_section)
{   
    /* DANGER!!! 
       Since the V2R_app_section_t object containing TOO-MUCH data in one single structure,
       current version of  "EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_rx)"
       ONLY wrap the NEEDED information for "EVSP" !!! .

       If future version need to pass more information to external app,
       (e.g., TSP need other entry in the V2R_app_section_t)
       use send_to_unix_socket_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_OBU_packet_rx)");
    if( ret ){
        return ret;
    }

    /* currently ARG_TYPE_OF(on_OBU_packet_rx) is the same type of V2R_app_section_t 
     * in future virsion, if you change the ARG_TYPE_OF(on_OBU_packet_rx) definition,
     * you need to copy the needed sections from V2R_app_section_t to ARG_TYPE_OF(on_OBU_packet_rx)
    */
    ARG_TYPE_OF(on_OBU_packet_rx) *app_section_p = (V2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    int buf_len;
    uint8_t *buf;
    J2735CodecErr err;
    char errmsg_buf[ERR_MSG_SZ];

    MessageFrame msgf;
    memset(&msgf, 0, sizeof(msgf));
    memset(&err, 0, sizeof(J2735CodecErr));

    err.msg_size = ERR_MSG_SZ;
    err.msg = errmsg_buf;

    msgf.messageId = app_section_p->msgID;
    msgf.u.data = app_section_p->data;
    buf_len = j2735_msg_encode(&buf, &msgf, &err);

    if (buf_len <= 0) {
        log_file_write_fatal_error("%s: j2735_msg_encode failed to encode msg\n", __func__);
        return EA_ERR_J2735_MSG_ENCODE;
    }

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_OBU_PACKET_RX;  

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        goto need_buf_free;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(ARG_TYPE_OF(on_OBU_packet_rx)));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        goto need_buf_free;
    }

    /* since there are inner structure inside, we send them here, one-by-one 
      the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app, 
       MUST "MATCH" the "send" action of these below 
    */

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section_p->payload", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->OBU_object , sizeof(OBU_object_t));
    if(ret){
        simple_fatal_act_logger("send app_section_p->OBU_object", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, 
                                 app_section_p->OBU_object->private_space, 
                                 sizeof(app_private_space_t));
    if(ret){
        simple_fatal_act_logger("send send OBU_object->private_space", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, buf_len, sizeof(int));
    if(ret){
        simple_fatal_act_logger("send len of buf", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, buf, buf_len);
    if(ret){
        simple_fatal_act_logger("send buf", ret);
        goto need_buf_free;
    }

need_buf_free: 
    free(buf);
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_tx)(void *app_section)
{
    /* DANGER!!! 
       Since the V2R_app_section_t object containing TOO-MUCH data in one single structure,
       current version of  "EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_tx)"
       ONLY wrap the NEEDED information for "EVSP" !!! .

       If future version need to pass more information to external app,
       (e.g., TSP need other entry in the V2R_app_section_t)
       use send_to_unix_socket_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_tx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_OBU_packet_tx)");
    if( ret ){
        return ret;
    }

    /* currently ARG_TYPE_OF(on_OBU_packet_tx) is the same type of V2R_app_section_t 
     * in future virsion, if you change the ARG_TYPE_OF(on_OBU_packet_tx) definition,
     * you need to copy the needed sections from V2R_app_section_t to ARG_TYPE_OF(on_OBU_packet_tx)
    */
    ARG_TYPE_OF(on_OBU_packet_tx) *app_section_p = (V2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    int buf_len;
    uint8_t *buf;
    J2735CodecErr err;
    char errmsg_buf[ERR_MSG_SZ];

    MessageFrame msgf;
    memset(&msgf, 0, sizeof(msgf));
    memset(&err, 0, sizeof(J2735CodecErr));

    err.msg_size = ERR_MSG_SZ;
    err.msg = errmsg_buf;

    msgf.messageId = app_section_p->msgID;
    msgf.u.data = app_section_p->data;
    buf_len = j2735_msg_encode(&buf, &msgf, &err);

    if (buf_len <= 0) {
        log_file_write_fatal_error("%s: j2735_msg_encode failed to encode msg\n", __func__);
        return EA_ERR_J2735_MSG_ENCODE;
    }

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_OBU_PACKET_TX;  

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        goto need_buf_free;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(ARG_TYPE_OF(on_OBU_packet_tx)));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        goto need_buf_free;
    }

    /* since there are inner structure inside, we send them here, one-by-one 
      the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_tx)"
       in the file of external_app_proxy_client.c, used by external app, 
       MUST "MATCH" the "send" action of these below 
    */

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section_p->payload", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->OBU_object , sizeof(OBU_object_t));
    if(ret){
        simple_fatal_act_logger("send app_section_p->OBU_object", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, 
                                 app_section_p->OBU_object->private_space, 
                                 sizeof(app_private_space_t));
    if(ret){
        simple_fatal_act_logger("send send OBU_object->private_space", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, buf_len, sizeof(int));
    if(ret){
        simple_fatal_act_logger("send len of buf", ret);
        goto need_buf_free;
    }

    ret = send_to_unix_socket_fd(notify_fd, buf, buf_len);
    if(ret){
        simple_fatal_act_logger("send buf", ret);
        goto need_buf_free;
    }

need_buf_free: 
    free(buf);
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_rx)(void *app_section)
{
    /* current version of middleware have not defined 
       the structure of R2R packet, so this callback 
       will not be registered , nor be called */
    fprintf(stderr,"WRAPPER_OF(on_RSU_packet_rx): should not be called at current verstion\n");
    log_file_write_fatal_error("WRAPPER_OF(on_RSU_packet_rx): should not be called at current verstion\n");
    return -1;
}

int EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_tx)(void *app_section){
    /* current version of middleware have not defined 
       the structure of R2R packet, so this callback 
       will not be registered , nor be called */
    fprintf(stderr,"WRAPPER_OF(on_RSU_packet_tx): should not be called at current verstion\n");
    log_file_write_fatal_error("WRAPPER_OF(on_RSU_packet_tx): should not be called at current verstion\n");
    return -1;
}

int EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_rx)(void *app_section)
{
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_cloud_packet_rx)") ){
        return ret;
    }

    C2R_app_section_t* app_section_p = (C2R_app_section_t*)app_section;
    
    /* if it is a packet about dontSend2TC , handle it in middleware before send out */
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    msg_buf_t read_buf;
    read_buf.index = 0;
    Malloc(read_buf.content, app_section_p->payload_len, "on_cloud_packet_rx");
    if (read_buf.content == NULL)
        return -1;
    memcpy(read_buf.content, app_section_p->payload, app_section_p->payload_len);
    
    uint8_t cmd; // read cmd
    read_uint8_t(&cmd, &read_buf);
    
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_rx): SPECIFIC FIELD\n");
        for (int i = 0; i < app_section_p->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_rx): CMD(%d)", cmd);

    log_file_write(log_content);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            proxy_handling_app_p->dontSend2TC == 0) 
        {  // enable/clear command buffer
            proxy_handling_app_p->dontSend2TC = 1;
            log_file_write("%s disable\r\n", proxy_handling_app_p->name);
            printf("evsp disable\r\n");
        } 
        else if (enableOrdisable == 2 &&
                 proxy_handling_app_p->dontSend2TC == 1) 
        {  // disable command buffer/then stop the command in
           // command buffer sent to tc machine
            proxy_handling_app_p->dontSend2TC = 0;
            //command_buf_clear(); // 永貞和學陽說這裡不應該 clear
            //log_file_write("evsp enable and command buffer clear\r\n");
            log_file_write("%s enable\r\n", proxy_handling_app_p->name);
        } else {
            log_file_write(
                "invalid cloud pcket disable/enable packet to tc machine\r\n");
        }
    } break;
    default:
        break;
    }
    
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }

    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;
    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_CLOUD_PACKET_RX; 

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section->payload", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_tx)(void *app_section)
{
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_cloud_packet_tx)") ){
        return ret;
    }

    C2R_app_section_t* app_section_p = (C2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_CLOUD_PACKET_TX;

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section_p->payload", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_camera_packet_rx)(void *app_section)
{   
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_camera_packet_rx)") ){
        return ret;
    }

    ARG_TYPE_OF(on_camera_packet_rx) *obstaclelist_p = (ObstacleList*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_CAMERA_PACKET_RX;

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, obstaclelist_p, sizeof(ObstacleList));
    if(ret){
        simple_fatal_act_logger("send obstaclelist", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_to_unix_socket_fd(notify_fd, obstaclelist_p->tab,
                                 (obstaclelist_p->count)*sizeof(Obstacle));
    if(ret){
        simple_fatal_act_logger("send obstaclelist->tab", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_traffic_signal_command_tx)(void *app_section)
{
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_traffic_signal_command_tx)") ){
        return ret;
    }

    traffic_signal_command_arg_t* app_section_p = (traffic_signal_command_arg_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_TRAFFIC_SIGNAL_COMMAND_TX;

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(traffic_signal_command_arg_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_registration)(void *app_section)
{
    /* for current version of middleware 
       external application will registered with the help of
       external_app_proxy server/client,
       so this callback should not be called */

    fprintf(stderr,"WRAPPER_OF(on_registration): should not be called at current verstion\n");
    log_file_write_fatal_error("WRAPPER_OF(on_registration): should not be called at current verstion\n");
    return -1;
}

int EAP_CALLBACK_WRAPPER_OF(on_middleware_restart)(void *app_section)
{   
    /* for current version of middleware 
       on_middle_restart() will NOT send data via app_section
       ( i.e., app_obj_t->on_middle_restart(NULL) )
       so we simply send header packet only; */
    
    int ret;
    
    if(!proxy_handling_app_p){
        fprintf(stderr,"WRAPPER_OF(on_middle_restart): proxy_handling_app_p be assigned NULL ptr!\n");
        log_file_write_fatal_error("WRAPPER_OF(on_middle_restart): proxy_handling_app_p be assigned NULL ptr!");
        return -1;
    }
    if( proxy_handling_app_p->ea_info_p == 0){
        fprintf(stderr,"WRAPPER_OF(on_middle_restart): be called when the app is not external\n");
        log_file_write_fatal_error("WRAPPER_OF(on_middle_restart): be called when the app is not external\n");
        return -1;
    }

    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_MIDDLEWARE_RESTART;

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_middleware_restart): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_middleware_restart):send header ret:%d\n", ret);
        return ret;
    }
    return ret;
}

/* NOTICE, if you add new event callback, you NEED to add a entry for the reconstruct function */
eap_callback_wrapper_fp callback_wrapper_fp_arr[EVENT_TYPE_NUMBER] = {
    [EVENT_OBU_PACKET_RX] = EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_rx),
    [EVENT_OBU_PACKET_TX] = EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_tx),
    [EVENT_RSU_PACKET_RX] = EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_rx),
    [EVENT_RSU_PACKET_TX] = EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_tx),
    [EVENT_CLOUD_PACKET_RX] = EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_rx),
    [EVENT_CLOUD_PACKET_TX] = EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_tx),
    [EVENT_TRAFFIC_SIGNAL_COMMAND_TX] = EAP_CALLBACK_WRAPPER_OF(on_traffic_signal_command_tx),
    [EVENT_CAMERA_PACKET_RX] = EAP_CALLBACK_WRAPPER_OF(on_camera_packet_rx),
    [EVENT_REGISTRATION] = EAP_CALLBACK_WRAPPER_OF(on_registration),
    [EVENT_MIDDLEWARE_RESTART] = EAP_CALLBACK_WRAPPER_OF(on_middleware_restart),
};