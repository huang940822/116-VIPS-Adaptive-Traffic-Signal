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

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"
#include "external_app_proxy_callback_wrapper.h"

/* NOTICE, if you add new callback, you NEED to add a new EAP_CALLBACK_WRAPPER */
/* and update the callback_wrapper_fp_arr[]  */

app_obj_t* proxy_handling_app_p;

#define EAP_CALLBACK_WRAPPER_OF(event_name) event_name ## _wrapper_callback

/* NOTICE, if you add new event callback, you NEED to add a related wrapper function */
int EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_rx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_OBU_packet_tx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_rx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_RSU_packet_tx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_rx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_cloud_packet_tx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_traffic_signal_command_tx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_camera_packet_rx)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_registration)(void *app_section);
int EAP_CALLBACK_WRAPPER_OF(on_middleware_restart)(void *app_section);

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
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_OBU_packet_rx)") ){
        return ret;
    }
    V2R_app_section_t* app_section_p = (V2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_mask = 0;   /* reset first */
    header.callback_mask |= ( 0x1 << BIT_SHIFT_FOR(on_OBU_packet_rx) );

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send header ret:%d\n", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(V2R_app_section_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send app_section ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send app_section ret:%d\n", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here, one-by-one 
      the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_rx)"
       in the file of external_app_proxy_client.c, used by external app, 
       MUST "MATCH" the "send" action of these below 
    */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send app_section_p->payload ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send app_section_p->payload ret:%d\n", ret);
        return ret;
    }

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->OBU_object , sizeof(OBU_object_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send app_section_p->OBU_object ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send app_section_p->OBU_object ret:%d\n", ret);
        return ret;
    }

    ret = send_to_unix_socket_fd(notify_fd, 
                                 app_section_p->OBU_object->private_space, 
                                 sizeof(app_private_space_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send OBU_object->private_space ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send OBU_object->private_space ret:%d\n", ret);
        return ret;
    }

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
    int ret;
    if( ret = eap_callback_wrapper_checker(app_section, "WRAPPER_OF(on_OBU_packet_tx)") ){
        return ret;
    }
    V2R_app_section_t* app_section_p = (V2R_app_section_t*)app_section;
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_mask = 0;   /* reset first */
    header.callback_mask |= ( 0x1 << BIT_SHIFT_FOR(on_OBU_packet_tx) );

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_tx): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_tx):send header ret:%d\n", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(V2R_app_section_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_tx): "
                       "send app_section ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_tx):send app_section ret:%d\n", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here, one-by-one 
      the "read" action of "RECONSTRUCT_PAYLOAD_FUNC_OF(on_OBU_packet_tx)"
       in the file of external_app_proxy_client.c, used by external app, 
       MUST "MATCH" the "send" action of these below 
    */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_tx): "
                       "send app_section_p->payload ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_tx):send app_section_p->payload ret:%d\n", ret);
        return ret;
    }

    ret = send_to_unix_socket_fd(notify_fd, app_section_p->OBU_object , sizeof(OBU_object_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_tx): "
                       "send app_section_p->OBU_object ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_tx):send app_section_p->OBU_object ret:%d\n", ret);
        return ret;
    }

    ret = send_to_unix_socket_fd(notify_fd, 
                                 app_section_p->OBU_object->private_space, 
                                 sizeof(app_private_space_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_tx): "
                       "send OBU_object->private_space ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_tx):send OBU_object->private_space ret:%d\n", ret);
        return ret;
    }

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
    int notify_fd = proxy_handling_app_p->ea_info_p->notify_fd;

    packet_from_proxy_header_t header;
    header.packet_type = EA_PACKET_TYPE_NM_NTF;
    header.callback_mask = 0;   /* reset first */
    header.callback_mask |= ( 0x1 << BIT_SHIFT_FOR(on_cloud_packet_rx) );

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_rx): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_rx):send header ret:%d\n", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_rx): "
                       "send app_section ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_rx):send app_section ret:%d\n", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_rx): "
                       "send app_section_p->payload ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_rx):send app_section_p->payload ret:%d\n", ret);
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
    header.callback_mask = 0;   /* reset first */
    header.callback_mask |= ( 0x1 << BIT_SHIFT_FOR(on_cloud_packet_tx) );

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_tx): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_tx):send header ret:%d\n", ret);
        return ret;
    }

    /* send toppest level structure */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p, sizeof(C2R_app_section_t));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_tx): "
                       "send app_section ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_tx):send app_section ret:%d\n", ret);
        return ret;
    }

    /* since there are inner structure inside, we send them here */
    ret = send_to_unix_socket_fd(notify_fd, app_section_p->payload , app_section_p->payload_len);
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_cloud_packet_tx): "
                       "send app_section_p->payload ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_cloud_packet_tx):send app_section_p->payload ret:%d\n", ret);
        return ret;
    }
    
    return ret;
}

int EAP_CALLBACK_WRAPPER_OF(on_camera_packet_rx)(void *app_section){
    ObstacleList *obstaclelist;
    ;
}

int EAP_CALLBACK_WRAPPER_OF(on_traffic_signal_command_tx)(void *app_section){
    traffic_signal_command_arg_t *command;
    ;
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
    header.callback_mask = 0;   /* reset first */
    header.callback_mask |= ( 0x1 << BIT_SHIFT_FOR(on_middleware_restart) );

    ret = send_to_unix_socket_fd(notify_fd, &header, sizeof(header));
    if(ret){
        fprintf(stderr,"WRAPPER_OF(on_OBU_packet_rx): "
                       "send header ret:%d\n", ret);
        log_file_write_fatal_error(
            "WRAPPER_OF(on_OBU_packet_rx):send header ret:%d\n", ret);
        return ret;
    }
    return ret;
}
