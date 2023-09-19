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
//#include "j2735inc/j2735_codec.h"

#include "external_app_proxy_socket.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_callback_msg_forward.h"

app_obj_t* proxy_handling_app_p;

static inline __attribute__((always_inline)) 
int pre_handling_cloud_packet_before_forwarding(C2R_app_section_t* app_section, app_obj_t* app_obj_p)
{
    /* this function is modified from "int EVSP_on_CLOUD_packet_rx(void *arg)" */

    /* since the "dontSend2TC" flag is actually valid inside middleware,
     * we handle it here before we do forwarding the cloud packet to external APP */

    msg_buf_t read_buf;
    read_buf.index = 0;
    Malloc(read_buf.content, app_section->payload_len, "FORWARD_FUNC_OF(on_cloud_packet_rx)");
    if (read_buf.content == NULL)
        return -1;
    memcpy(read_buf.content, app_section->payload, app_section->payload_len);
    
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            app_obj_p->dontSend2TC == 0) {  // enable/clear command buffer
            app_obj_p->dontSend2TC = 1;
            log_file_write("%s disable\r\n", app_obj_p->name);
            printf("%s disable\r\n", app_obj_p->name);
        } 
        else if (enableOrdisable == 2 
                 && app_obj_p->dontSend2TC == 1)          
        {  // disable command buffer/then stop the command in
           // command buffer sent to tc machine
            app_obj_p->dontSend2TC = 0;
            //command_buf_clear();
            log_file_write("%s enabled\r\n", app_obj_p->name);
            printf("%s enabled\r\n", app_obj_p->name);
        } else {
            log_file_write(
                "invalid cloud pcket disable/enable packet to tc machine\r\n");
        }
        // printf("not implement evsp on cloud rx action yet when cmd is
        // 0\r\n");
    } break;
    default:
        break;
    }
    return 0;
}

static inline __attribute__((always_inline)) 
int forward_function_parameter_check(void *app_section, event_type_t event, char* func_name)
{
    if(!app_section){
        /* in currnent design of middleware, some of callbacks do not have parameter passed in */
        /* this error is not fatal for middleware itself, but the app callback will not be called */
        if( event != EVENT_MIDDLEWARE_RESTART 
            && event != EVENT_REGISTRATION )
        {
            fprintf(stderr,"%s: app_section assigned is NULL ptr!, "
                           "skip callback parameter forwarding\n", func_name);
            log_file_write("%s: app_section assigned is NULL ptr!, "
                           "skip callback parameter forwarding\n", func_name);
        }
        return -1;
    }
    if(!proxy_handling_app_p){
        fprintf(stderr,"%s: proxy_handling_app_p assigned is NULL ptr!\n", func_name);
        /* this error is fatal. it should never happend. if detected, check the implementation */
        log_file_write_fatal_error("%s: proxy_handling_app_p assigned is NULL ptr!\n", func_name);
        return -1;
    }
    if( proxy_handling_app_p->ea_info_p == 0){
        fprintf(stderr,"%s: be called when the app is not external\n", func_name);
        /* this error is fatal. it should never happend. if detected, check the implementation */
        log_file_write_fatal_error("%s: be called when the app is not external\n", func_name);
        return -1;
    }
    if( proxy_handling_app_p->ea_info_p->notify_fd == 0){
        // fprintf(stderr,"%s: external APP's notify_fd is 0, probably disconnected\n", func_name);
        /* this error is not fatal */
        log_file_write("%s: external APP's notify_fd is 0, probably disconnected\n", func_name);
        return -1;
    }
    return 0;
}

static inline __attribute__((always_inline)) 
int simple_send_notify_header(int fd, event_type_t event)
{
    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = event;  
    return send_packet_to_unix_sk_fd(fd, &header, sizeof(header));
}


/* NOTICE, if you add new callback, you NEED to add a new EAP_CBMSG_FORWARD_FUNC_OF */
/* and update the cbmsg_forward_fp_arr[]  */

/* below are all the callback forward functions for all event callback */
/* NOTICE, if you add new event callback, you NEED to add a related forward function */
#define EAP_CBMSG_FORWARD_FUNC_OF(event_name) event_name ## _cbmsg_forward_func

int EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_rx)(void *app_section)
{   
    record_current_timespec(&trc3);
    /* DANGER!!! 
       If future version need to pass more information to external app,
       (e.g., TSP need other entry in the V2R_app_section_t)
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this forward-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_OBU_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_OBU_PACKET_RX,
                                               "FORWARD_FUNC_OF(on_OBU_packet_rx)");
    if( ret ){
        return ret;
    }
    
    /* currently the app_section passed into FORWARD_FUNC_OF(on_OBU_packet_rx) 
       is wrapper_arg_for_obu_packet_t* */
    wrapper_arg_for_obu_packet_t* arg_p = (wrapper_arg_for_obu_packet_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    ret = simple_send_notify_header(notify_fd, EVENT_OBU_PACKET_RX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, &(arg_p->msg_p->msg_len), sizeof(size_t));
    if(ret){
        simple_fatal_act_logger("send msg_len", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->msg_p->msg , arg_p->msg_p->msg_len);
    if(ret){
        simple_fatal_act_logger("send msg", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->object_p , sizeof(OBU_object_t));
    if(ret){
        simple_fatal_act_logger("send OBU_object", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->object_p->private_space , sizeof(app_private_space_t));
    if(ret){
        simple_fatal_act_logger("send private_space", ret);
        return ret;
    }

    record_current_timespec(&trc4);
    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_tx)(void *app_section)
{
    /* DANGER!!! 
       If future version need to pass more information to external app,
       (e.g., TSP need other entry in the V2R_app_section_t)
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this forward-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_OBU_packet_tx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_OBU_PACKET_TX,
                                               "FORWARD_FUNC_OF(on_OBU_packet_tx)");
    if( ret ){
        return ret;
    }

    /* currently the app_section passed into FORWARD_FUNC_OF(on_OBU_packet_tx) 
       is wrapper_arg_for_obu_packet_t* */
    wrapper_arg_for_obu_packet_t* arg_p = (wrapper_arg_for_obu_packet_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    ret = simple_send_notify_header(notify_fd, EVENT_OBU_PACKET_RX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, &(arg_p->msg_p->msg_len), sizeof(size_t));
    if(ret){
        simple_fatal_act_logger("send msg_len", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->msg_p->msg , arg_p->msg_p->msg_len);
    if(ret){
        simple_fatal_act_logger("send msg", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->object_p , sizeof(OBU_object_t));
    if(ret){
        simple_fatal_act_logger("send OBU_object", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, arg_p->object_p->private_space , sizeof(app_private_space_t));
    if(ret){
        simple_fatal_act_logger("send private_space", ret);
        return ret;
    }

    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_RSU_packet_rx)(void *app_section)
{
    /* current version of middleware have not defined 
       the structure of R2R packet, so this callback 
       will not be registered , nor be called */
    fprintf(stderr,"FORWARD_FUNC_OF(on_RSU_packet_rx): should not be called at current verstion\n");
    log_file_write_fatal_error("FORWARD_FUNC_OF(on_RSU_packet_rx): should not be called at current verstion\n");
    return -1;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_RSU_packet_tx)(void *app_section){
    /* current version of middleware have not defined 
       the structure of R2R packet, so this callback 
       will not be registered , nor be called */
    fprintf(stderr,"FORWARD_FUNC_OF(on_RSU_packet_tx): should not be called at current verstion\n");
    log_file_write_fatal_error("FORWARD_FUNC_OF(on_RSU_packet_tx): should not be called at current verstion\n");
    return -1;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_cloud_packet_rx)(void *app_section)
{
    /* DANGER!!! 
       If future version need to pass more information to external app,
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_cloud_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_CLOUD_PACKET_RX,
                                               "FORWARD_FUNC_OF(on_cloud_packet_rx)");
    if( ret ){
        return ret;
    }

    C2R_app_section_t* app_section_p = (C2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    /* since the "dontSend2TC" flag is actually valid inside middleware,
     * we handle it here before we do forwarding the cloud packet to external APP */
    ret = pre_handling_cloud_packet_before_forwarding(app_section_p, proxy_handling_app_p);
    if( ret ){
        return ret;
    }

    ret = simple_send_notify_header(notify_fd, EVENT_CLOUD_PACKET_RX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_packet_to_unix_sk_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_packet_to_unix_sk_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section->payload", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_cloud_packet_tx)(void *app_section)
{
    /* DANGER!!! 
       If future version need to pass more information to external app,
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_cloud_packet_tx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_CLOUD_PACKET_TX,
                                               "FORWARD_FUNC_OF(on_cloud_packet_tx)");
    if( ret ){
        return ret;
    }

    C2R_app_section_t* app_section_p = (C2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    ret = simple_send_notify_header(notify_fd, EVENT_CLOUD_PACKET_RX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_packet_to_unix_sk_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_packet_to_unix_sk_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        simple_fatal_act_logger("send app_section->payload", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_camera_packet_rx)(void *app_section)
{   
    /* DANGER!!! 
       If future version need to pass more information to external app,
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_cloud_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_CAMERA_PACKET_RX,
                                               "FORWARD_FUNC_OF(on_camera_packet_rx)");
    if( ret ){
        return ret;
    }

    ObstacleList* obstaclelist_p = (ObstacleList*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    ret = simple_send_notify_header(notify_fd, EVENT_CAMERA_PACKET_RX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_packet_to_unix_sk_fd(notify_fd, obstaclelist_p, sizeof(ObstacleList));
    if(ret){
        simple_fatal_act_logger("send obstaclelist", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_packet_to_unix_sk_fd(notify_fd, obstaclelist_p->tab,
                                 (obstaclelist_p->count)*sizeof(Obstacle));
    if(ret){
        simple_fatal_act_logger("send obstaclelist->tab", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_traffic_signal_command_tx)(void *app_section)
{
    /* DANGER!!! 
       If future version need to pass more information to external app,
       use send_packet_to_unix_sk_fd() to send more data,
       and make sure the "send" action of this wrapper-function
       "MATCH" the "read" action of "RECONSTRUCT_MSG_FUNC_OF(on_traffic_signal_command_tx)"
       in the file of external_app_proxy_client.c, used by external app.
    */
    int ret = forward_function_parameter_check(app_section, EVENT_TRAFFIC_SIGNAL_COMMAND_TX,
                                               "FORWARD_FUNC_OF(on_traffic_signal_command_tx)");
    if( ret ){
        return ret;
    }

    traffic_signal_command_arg_t* app_section_p = (traffic_signal_command_arg_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    ret = simple_send_notify_header(notify_fd, EVENT_TRAFFIC_SIGNAL_COMMAND_TX);
    if(ret){
        simple_fatal_act_logger("send header", ret);
        return ret;
    }

    ret = send_packet_to_unix_sk_fd(notify_fd, app_section_p, sizeof(traffic_signal_command_arg_t));
    if(ret){
        simple_fatal_act_logger("send app_section", ret);
        return ret;
    }

    return ret;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_registration)(void *app_section)
{
    /* for current version of middleware 
       external application will registered with the help of
       external_app_proxy server/ external_app_proxy client-library,
       and the on_registration callback will directly be invoked on client side,
       so this callback should not be called */

    fprintf(stderr,"FORWARD_FUNC_OF(on_registration): should not be called at current verstion\n");
    log_file_write_fatal_error("FORWARD_FUNC_OF(on_registration): should not be called at current verstion\n");
    return -1;
}

int EAP_CBMSG_FORWARD_FUNC_OF(on_middleware_restart)(void *app_section)
{   
    /* for current version of middleware 
       on_middle_restart() will NOT send data via app_section
       ( i.e., app_obj_t->on_middle_restart(NULL) )
       so we simply send header packet only; */
    
    int ret;
    
    if(!proxy_handling_app_p){
        fprintf(stderr,"FORWARD_FUNC_OF(on_middle_restart): proxy_handling_app_p be assigned NULL ptr!\n");
        log_file_write_fatal_error("FORWARD_FUNC_OF(on_middle_restart): proxy_handling_app_p be assigned NULL ptr!");
        return -1;
    }
    if( proxy_handling_app_p->ea_info_p == 0){
        fprintf(stderr,"FORWARD_FUNC_OF(on_middle_restart): be called when the app is not external\n");
        log_file_write_fatal_error("FORWARD_FUNC_OF(on_middle_restart): be called when the app is not external\n");
        return -1;
    }

    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_event = EVENT_MIDDLEWARE_RESTART;

    ret = send_packet_to_unix_sk_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"FORWARD_FUNC_OF(on_middleware_restart): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "FORWARD_FUNC_OF(on_middleware_restart):send header ret:%d\n", ret);
        return ret;
    }
    return ret;
}

/* NOTICE, if you add new event callback, you NEED to add a entry for the cbmsg_forward function */
/* "eap" stands for "external application proxy" */
/* "cbmsg" stands for "callback message" */
eap_cbmsg_forward_fp cbmsg_forward_fp_arr[EVENT_TYPE_NUMBER] = {
    [EVENT_OBU_PACKET_RX] = EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_rx),
    [EVENT_OBU_PACKET_TX] = EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_tx),
    [EVENT_RSU_PACKET_RX] = EAP_CBMSG_FORWARD_FUNC_OF(on_RSU_packet_rx),
    [EVENT_RSU_PACKET_TX] = EAP_CBMSG_FORWARD_FUNC_OF(on_RSU_packet_tx),
    [EVENT_CLOUD_PACKET_RX] = EAP_CBMSG_FORWARD_FUNC_OF(on_cloud_packet_rx),
    [EVENT_CLOUD_PACKET_TX] = EAP_CBMSG_FORWARD_FUNC_OF(on_cloud_packet_tx),
    [EVENT_TRAFFIC_SIGNAL_COMMAND_TX] = EAP_CBMSG_FORWARD_FUNC_OF(on_traffic_signal_command_tx),
    [EVENT_CAMERA_PACKET_RX] = EAP_CBMSG_FORWARD_FUNC_OF(on_camera_packet_rx),
    [EVENT_REGISTRATION] = EAP_CBMSG_FORWARD_FUNC_OF(on_registration),
    [EVENT_MIDDLEWARE_RESTART] = EAP_CBMSG_FORWARD_FUNC_OF(on_middleware_restart),
};