#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/timerfd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/epoll.h> // for epoll_create1()

#include "log.h"
#include "typedefine.h"
#include "application_registration.h"
#include "application_management_helper.h"
#include "config.h"

#include "external_app_proxy_socket.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_api_wrapper.h"
#include "external_app_proxy_callback_forward.h"
#include "external_app_proxy_server.h"

#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
#define EPOLL_MAX_EVENTS 64

/* The backlog argument defines the maximum length to which the
 * queue of pending connections for sockfd may grow. 
 * Reference: https://man7.org/linux/man-pages/man2/listen.2.html */
#define MY_BACKLOG 20

static int current_errno;   //record errno for failed syscall 
static int unix_listen_fd;  //only one, used for accepting new client
static int ep_fd;           //fd for epoll
static uint8_t proxy_cur_heartbeat;    
static int check_heartbeat_timer_fd;      //timer-fd for check app's heartbeat periodically
static struct itimerspec check_heartbeat_its;   //itimerspec used for timer-fd above

int32_t recv_packet_from_unix_sk_fd(int socket_fd, void* packet_p, size_t packet_size);
int32_t send_packet_to_unix_sk_fd(int socket_fd, void* packet_p, size_t packet_size);

/* the callback_forward_fp_arr[] will be used by "despather", "indirectly" 
 * NOTICE, if you add new callback, you NEED to update 
 * this function: inner_set_external_app_callback_by_mask() */
static inline __attribute__((always_inline)) 
void inner_set_external_app_callback_by_mask(app_obj_t* app_obj_p, uint64_t mask)
{
    if( mask & ( 0x1 << EVENT_OBU_PACKET_RX ) ){
        app_obj_p->on_OBU_packet_rx = callback_forward_fp_arr[EVENT_OBU_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_OBU_PACKET_TX ) ){
        app_obj_p->on_OBU_packet_tx = callback_forward_fp_arr[EVENT_OBU_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_RSU_PACKET_RX ) ){
        app_obj_p->on_RSU_packet_rx = callback_forward_fp_arr[EVENT_RSU_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_RSU_PACKET_TX ) ){
        app_obj_p->on_RSU_packet_tx = callback_forward_fp_arr[EVENT_RSU_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_CLOUD_PACKET_RX ) ){
        app_obj_p->on_cloud_packet_rx = callback_forward_fp_arr[EVENT_CLOUD_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_CLOUD_PACKET_TX ) ){
        app_obj_p->on_cloud_packet_tx = callback_forward_fp_arr[EVENT_CLOUD_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_TRAFFIC_SIGNAL_COMMAND_TX ) ){
        app_obj_p->on_traffic_signal_command_tx = callback_forward_fp_arr[EVENT_TRAFFIC_SIGNAL_COMMAND_TX];
    }
    if( mask & ( 0x1 << EVENT_CAMERA_PACKET_RX ) ){
        app_obj_p->on_camera_packet_rx = callback_forward_fp_arr[EVENT_CAMERA_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_REGISTRATION ) ){
        /* since for external application,
           the on_registration callback will be directly called at client side 
           we just IGNORE the on_registration callback at server side */
        //app_obj_p->on_registration = callback_forward_fp_arr[EVENT_REGISTRATION];
        ; //do nothing in current version
    }
    if( mask & ( 0x1 << EVENT_MIDDLEWARE_RESTART ) ){
        app_obj_p->on_middleware_restart = callback_forward_fp_arr[EVENT_MIDDLEWARE_RESTART];
    }
}

static inline  __attribute__((always_inline))  
int set_timer_fd_for_checking_app_hearbeat()
{   
    check_heartbeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (check_heartbeat_timer_fd == -1) {
        log_file_write_fatal_error(
            "set_timer_fd_for_checking_app_hearbeat()"
            "timerfd_create with check_heartbeat_timer_fd ret %d\n", check_heartbeat_timer_fd);
        return -1;
    }

    check_heartbeat_its.it_interval.tv_sec = HEARTBEAT_CHECK_PERIOD_S;
    check_heartbeat_its.it_interval.tv_nsec = 0;
    check_heartbeat_its.it_value.tv_sec = HEARTBEAT_CHECK_START_OFFSET_S;
    check_heartbeat_its.it_value.tv_nsec = 0;
    
    int ret = timerfd_settime(check_heartbeat_timer_fd, 0, &check_heartbeat_its, NULL);
    if ( ret < 0) {
        log_file_write_fatal_error(
            "set_timer_fd_for_checking_app_hearbeat()"
            "timerfd_settime with check_heartbeat_its ret %d\n", ret);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = check_heartbeat_timer_fd;
    ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, check_heartbeat_timer_fd, &ev);
    if( ret != 0 ){
        log_file_write_fatal_error(
            "set_timer_fd_for_checking_app_hearbeat()"
            "epoll_ctl(EPOLL_CTL_ADD) ret %d\n", ret);
        return -1;
    }

    return 0;
}

static inline  __attribute__((always_inline))  
int remove_both_channels_from_proxy_epoll(app_obj_t *app_obj_p)
{   
    /* After close the socket fd, 
     * it will automatically deregister itself from epoll,
     * so currently we do nothing in this function.
     * if in later linux version, the behavior of epoll changed,
     * this function might need to be implemented */
    return 0;
}

static inline  __attribute__((always_inline))  
int unlink_an_external_app(app_obj_t *app_obj_p)
{
    int ret;
    ret = remove_both_channels_from_proxy_epoll(app_obj_p);
    if(ret){
        log_file_write(
            "[EAP msg] err: %s call: remove_both_channels_from_proxy_epoll() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);
    }
    ret = close_both_channels_of_an_external_app(app_obj_p);
    if(ret){
        log_file_write(
            "[EAP msg] err: %s call: close_both_channels_of_an_external_app() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);

        /* even the close failed, we still reset the fds, to prevent proxy interact with it */
        app_obj_p->ea_info_p->interact_fd = 0;  
        app_obj_p->ea_info_p->notify_fd = 0;
    }
    return ret;
}

static inline  __attribute__((always_inline))  
int check_all_external_app_heartbeats()
{
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    int ret = 0;
    uint8_t record_diff;
    pthread_mutex_lock(&mutex_app_list); 

    app_obj_t *current = app_list.next;
    
    if (current == NULL) {      
        ret = 0;   /* empty list */
    }
    else{
        /* traverse to last node */
        while (current != NULL) {
            if( current->ea_info_p == 0){
                /* this app is not external */
                ;//do nothing
            }
            else if( current->ea_info_p->interact_fd == 0 
                     && current->ea_info_p->notify_fd == 0 )
            {
                /* this app already be unlinked */
                ;//do nothing
            }
            else{
                record_diff = proxy_cur_heartbeat - current->ea_info_p->heartbeat_rc;
                if ( record_diff > HEARTBEAT_CHECK_ALLOWED_THERSHHOLD ) {
                    unlink_an_external_app(current);

                    fprintf(stderr,
                        "[EAP msg] %s call: unlink_an_external_app() for appID:%d, ret = %d\n",
                        __func__, current->id, ret);
                    log_file_write(
                        "[EAP msg] %s call: unlink_an_external_app() for appID:%d, ret = %d\n",
                        __func__, current->id, ret);
                }
            }
            current = current->next;
        }
    }

    pthread_mutex_unlock(&mutex_app_list); 

    proxy_cur_heartbeat += 1;   
    return ret;
}

static inline __attribute__((always_inline)) 
int inner_handle_app_register( app_obj_t* app_p, void *payload_p)
{   
    app_registration_payload_t *pl_p = (app_registration_payload_t*)(payload_p);
    strncpy( app_p->name, pl_p->name, APP_NAME_MAX_LEN);
    app_p->dontSend2TC = pl_p->dontSend2TC;
    app_p->id = pl_p->id;
    app_p->priority = pl_p->priority;
    inner_set_external_app_callback_by_mask(app_p, pl_p->callback_register_mask);
    return app_register(app_p);  /* using existed function in application_registration.c */
}

static inline __attribute__((always_inline)) 
void print_and_log_external_app_register_msg( app_obj_t* app_p )
{   
    if(!app_p)
        return;

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    printf("\n[EAP msg] external app registered:\n");
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\n[EAP msg] external app registered:\n");

    printf(" app name: %s\n app id: %d\n app priority: %d\n", 
            app_p->name, app_p->id, app_p->priority);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             " app name: %s\n app id: %d\n app priority: %d\n", 
             app_p->name, app_p->id, app_p->priority);
    
    printf("callback registered initially:\n");
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "callback registered initially:\n");
    
    if(app_p->on_OBU_packet_rx){
        printf("- on_OBU_packet_rx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_OBU_packet_rx\n");
    }
    if(app_p->on_OBU_packet_tx){
        printf("- on_OBU_packet_tx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_OBU_packet_tx\n");
    }
    if(app_p->on_RSU_packet_rx){
        printf("- on_RSU_packet_rx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_RSU_packet_rx\n");
    }
    if(app_p->on_RSU_packet_tx){
        printf("- on_RSU_packet_tx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_RSU_packet_tx\n");
    }
    if(app_p->on_cloud_packet_rx){
        printf("- on_cloud_packet_rx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_cloud_packet_rx\n");
    }
    if(app_p->on_cloud_packet_tx){
        printf("- on_cloud_packet_tx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_cloud_packet_tx\n");
    }
    if(app_p->on_camera_packet_rx){
        printf("- on_camera_packet_rx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_camera_packet_rx\n");
    }
    if(app_p->on_traffic_signal_command_tx){
        printf("- on_traffic_signal_command_tx\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_traffic_signal_command_tx\n");
    }
    if(app_p->on_middleware_restart){
        printf("- on_middleware_restart\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "- on_middleware_restart\n");
    }

    printf("\n");
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\n");

    log_file_write(log_content);
}

/* this function will use api_wrapper_fp_arr, which will send unix packet to external app */
static inline __attribute__((always_inline)) 
int inner_handle_request_by_api_id(int client_fd, uint32_t api_id)
{   
    if ( client_fd == 0 ){
        fprintf(stderr, "err: inner_handle_request_by_api_id: client_fd == 0\n");
        return -1;
    }
    if ( api_id == API_ID_OF(special_reserved_api_id) ){
        fprintf(stderr, "err: inner_handle_request_by_api_id: api_id == %d\n", API_ID_OF(special_reserved_api_id));
        return -1;
    }
    if ( api_id >= NUM_OF_API_ID_DEFININITION ){
        fprintf(stderr, "err: inner_handle_request_by_api_id: api_id >= %d\n", NUM_OF_API_ID_DEFININITION);
        return -1;
    }
    if ( api_id == API_ID_OF(remote_app_registration) ){
        fprintf(stderr, "err: inner_handle_request_by_api_id: api_id == %d\n", API_ID_OF(remote_app_registration));
        return -1;
    }
    if ( api_id == API_ID_OF(app_main_loop_start) ){
        fprintf(stderr, "err: inner_handle_request_by_api_id: api_id == %d\n", API_ID_OF(app_main_loop_start));
        return -1;
    }

    /* call the related wrapper function by its api_id */
    int ret = (*api_wrapper_fp_arr[api_id])(client_fd);
    // maybe log the ret value
    
    if( ret == EAL_ERR_SOCKET_DISCONNECT ){
        ;  //TODO //maybe de-register the app
    }

    return ret;
}

static inline __attribute__((always_inline)) 
int inner_handle_heartbeat_from_app(int client_fd, packet_to_proxy_header_t *header_p)
{   
    packet_from_proxy_header_t ack;
    ack.packet_type = EA_PACKET_TYPE_ACK;
    ack.seq_num = header_p->seq_num;

    int ret = update_external_app_heartbeat_by_appID(header_p->appID);
    if(ret){
        ack.ret_val = EAL_ERR_IN_MIDDLEWARE_HEARTBEAT_UPDATE_FOR_APP;
    }
    else{
        ack.ret_val = EAL_ERR_OK;
    }
    
    ret = send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack));
    if(ret != 0){
        fprintf(stderr, "%s: send ack with seq_num:%u to client_fd:%d fail, ret = %d\n", 
                __func__, ack.seq_num, client_fd, ret);
        
        log_file_write("%s: send ack with seq_num:%u to client_fd:%d fail, ret = %d\n", 
                        __func__, ack.seq_num, client_fd, ret);
    }
    return ret;
}

uint8_t get_current_eap_heartbeat_rc()
{
    return proxy_cur_heartbeat;
}

int handle_new_client_accepted(int client_fd)
{
    /* WARNNING!!
     * make sure the packet you send-to/recv-from proxy client 
     * "MATCH" the send-to/recv-from action 
     *  in remote_app_registration() in external_app_proxy_client.c
     */

    int ret;
    packet_to_proxy_header_t header;
    packet_from_proxy_header_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.packet_type = EA_PACKET_TYPE_ACK;

    ret = recv_packet_from_unix_sk_fd( client_fd, &header, sizeof(header));
    if( ret != 0 ){
        fprintf( stderr, "%d: recv_packet_from_unix_sk_fd ret: %d\n",__LINE__ , ret);
        return ret;
    }
    /* handle header error-check */
    if( header.packet_type != EA_PACKET_TYPE_REGISTER
        && header.packet_type != EA_PACKET_TYPE_NTF_UPDATE)
    {   
        ack.ret_val = EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER;
        send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack));
        return 0;
    }
    else if( header.api_id != API_ID_OF(remote_app_registration)){
        fprintf( stderr, "register packet BAD_API_ID\n");
        ack.ret_val = EAL_ERR_BAD_API_ID_BEFORE_REGISTER;
        send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack));
        return 0;
    }
    /* then read payload */

    if( header.packet_type == EA_PACKET_TYPE_REGISTER ){
        /* new registration or re-registration */
        app_registration_payload_t payload;

        if( (ret = recv_packet_from_unix_sk_fd( client_fd, &payload, sizeof(payload)) ) != 0){
            fprintf( stderr, "%d: recv_packet_from_unix_sk_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }

        app_obj_t* in_middleware_app_p;
        in_middleware_app_p = get_app_obj_by_appID(payload.id);
        if( !in_middleware_app_p ){  /* brand-new app registration */
            Malloc(in_middleware_app_p, sizeof(app_obj_t), "in_middleware_app_p");
            Malloc(in_middleware_app_p->ea_info_p, sizeof(ea_info_t), "in_middleware_app_p->ea_info_p");
            ret = inner_handle_app_register(in_middleware_app_p, &payload);
            if(ret != 0){
                fprintf( stderr, "inner_handle_app_register ret %d\n", ret);
                ack.ret_val = EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT;
                send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack));
                return 0;
            }
        }
        
        ret = reset_external_app_both_fd(in_middleware_app_p, client_fd);  /*this will set notify_fd 0*/
        in_middleware_app_p->ea_info_p->heartbeat_rc = proxy_cur_heartbeat;
        in_middleware_app_p->ea_info_p->pid = payload.pid;


        packet_from_proxy_header_t ack;
        
        ack.ret_val = EAL_ERR_OK;
        if( (ret = send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack)) ) != 0){
            fprintf( stderr, "%d: send_packet_to_unix_sk_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }
    }
    else{  /* header.packet_type == EA_PACKET_TYPE_NTF_UPDATE */
        /* update notify channel after registration packet handled */
        notify_update_payload_t ntf_payload;
        packet_from_proxy_header_t ntf_ack;
        ntf_ack.packet_type = EA_PACKET_TYPE_ACK;

        if( (ret = recv_packet_from_unix_sk_fd( client_fd, &ntf_payload, sizeof(ntf_payload)) ) != 0){
            fprintf( stderr, "%d: recv_packet_from_unix_sk_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }

        app_obj_t* in_middleware_app_p;
        in_middleware_app_p = get_app_obj_by_appID(ntf_payload.id);
        if( !in_middleware_app_p ){
            /* NOTIFY PACKET should not come from a new app */
            ntf_ack.ret_val = EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE;
        }
        else if( in_middleware_app_p->ea_info_p->pid != ntf_payload.pid ) {
            /* NOTIFY PACKET should not come from another process 
             * different from one who link interact channel 
             */
            ntf_ack.ret_val = EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE;
        }
        else{
            ntf_ack.ret_val = EAL_ERR_OK;
            update_external_app_notify_fd(in_middleware_app_p, client_fd);
            in_middleware_app_p->ea_info_p->heartbeat_rc = proxy_cur_heartbeat;
        }

        if( (ret = send_packet_to_unix_sk_fd( client_fd, &ntf_ack, sizeof(ntf_ack)) ) != 0){
            fprintf( stderr, "%d: send_packet_to_unix_sk_fd ret: %d\n", __LINE__ , ret);
            return ret;
        }

        if(ntf_ack.ret_val == EAL_ERR_OK){
            notify_update_ack_payload_t ntf_ack_payload;
            ntf_ack_payload.RSU_id = config.RSU_id;
            ntf_ack_payload.RSU_lat = config.RSU_lat;
            ntf_ack_payload.RSU_lon = config.RSU_lon;
            ntf_ack_payload.RSU_elev = config.RSU_elev;
            ntf_ack_payload.RSU_region = config.RSU_region;
            strncpy(ntf_ack_payload.RSU_name, config.RSU_name, RSU_NAME_MAX_LEN);
            if( (ret = send_packet_to_unix_sk_fd( client_fd, &ntf_ack_payload, sizeof(ntf_ack_payload)) ) != 0){
                fprintf( stderr, "%d: send_packet_to_unix_sk_fd ret: %d\n", __LINE__ , ret);
                return ret;
            }
        }
        print_and_log_external_app_register_msg(in_middleware_app_p);
    }
    return ret;
}

int handle_remote_client_request(int client_fd)
{
    int ret;
    packet_to_proxy_header_t header;

    ret = recv_packet_from_unix_sk_fd( client_fd, &header, sizeof(header));
    if (ret != 0) {
        if(ret!=0)
        fprintf(stdout, "%s: recv_packet_from_unix_sk_fd header from client_fd:%d, ret = %d\n", 
                __func__, client_fd, ret);
        if(ret == EAL_ERR_SOCKET_DISCONNECT){
            fprintf(stdout, "%s: close disconnected external app client_fd:%d\n", 
                __func__, client_fd);
            close(client_fd);
            //de_registrate the app
        }
        return ret;
    }

    /* header error-check */
    if ( header.packet_type != EA_PACKET_TYPE_REQ 
         && header.packet_type != EA_PACKET_TYPE_HEARTBEAT) 
    {   
        packet_from_proxy_header_t ack;
        ack.packet_type = EA_PACKET_TYPE_ACK;
        ack.ret_val = EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE;
        ret = send_packet_to_unix_sk_fd( client_fd, &ack, sizeof(ack));
        if(ret!=0){
            fprintf(stdout, "%s: send ack to client_fd:%d, ret = %d\n", 
                    __func__, client_fd, ret);
            ;//maybe log err
        }
    }
    else if ( header.packet_type == EA_PACKET_TYPE_HEARTBEAT ) {
        #ifdef EAP_SERVER_PRINT_DEBUG
            printf("[EAP msg] get heartbeat packet from appID:%u, seq_num:%u\n", 
                    header.appID, header.seq_num);
        #endif
        log_file_write("[EAP msg] get heartbeat packet from appID:%u, seq_num:%u\n", 
                        header.appID, header.seq_num);

        ret = inner_handle_heartbeat_from_app(client_fd, &header);
    }
    else{   /* i.e., header.packet_type == EA_PACKET_TYPE_REQ */
        #ifdef EAP_SERVER_PRINT_DEBUG
            printf("[EAP msg] get request packet from appID:%u, api_id:%d ->%s() \n", 
                    header.appID, header.api_id, api_id_str_arr[header.api_id] );
        #endif
        log_file_write("[EAP msg] get request packet from appID:%u, api_id:%d ->%s() \n", 
                        header.appID, header.api_id, api_id_str_arr[header.api_id] );

        ret = inner_handle_request_by_api_id(client_fd, header.api_id);
    }

    if( ret == EAL_ERR_SOCKET_DISCONNECT ){
        app_obj_t *app_obj_p = get_app_obj_by_unix_socket_fd(client_fd);
        if(app_obj_p){
            pthread_mutex_lock(&mutex_app_list); 
            unlink_an_external_app(app_obj_p);
            pthread_mutex_unlock(&mutex_app_list); 
        }
        else{
            /* this case should never happend */
            #ifdef EAP_SERVER_PRINT_DEBUG
                printf(
                    "[EAP msg] get_app_obj_by_unix_socket_fd() cannot find matching app with client_fd: %d\n", 
                    client_fd );
            #endif
            log_file_write(
                "[EAP msg] get_app_obj_by_unix_socket_fd() cannot find matching app with client_fd: %d\n", 
                client_fd );
        }
    }
    return ret;
}

/* the external_app_proxy server "main thread" */
void *external_app_proxy_main_handler()
{   
    /* step0: create a unix domain socket with MY_UNIX_SOCKET_PATH 
     * unlink, if socket already exists */
    struct stat statbuf;
    if( stat (MY_UNIX_SOCKET_PATH, &statbuf) == 0) {
        log_file_write("[EAP MSG] %s: MY_UNIX_SOCKET_PATH is already exist\n" 
                       "will call unlink() before re-link\n", __func__);
        fprintf(stdout, "[EAP MSG] %s: MY_UNIX_SOCKET_PATH is already exist\n" 
                        "will call unlink() before re-link\n", __func__);
        if (unlink (MY_UNIX_SOCKET_PATH) == -1){
	        log_file_write_fatal_error("%s: unlink", __func__);
            fprintf(stderr, "err: %s: unlink\n", __func__);
        }
    }

    /* step1: create a unix domain socket */
    if( (unix_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
	    log_file_write_fatal_error("%s: socket", __func__);
        fprintf(stderr, "err: $%s: socket\n", __func__);
    }

    /* step2: bind the socket to MY_UNIX_SOCKET_PATH */
    /* sun_path in a char array with size = 108, be care of your "sk_addr.sun_path" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    if( bind(unix_listen_fd, (const struct sockaddr *) &sk_addr, sizeof (struct sockaddr_un)) == -1){
        log_file_write_fatal_error("%s: bind", __func__);
        fprintf(stderr, "err: %s: bind\n", __func__);
    }

    /* step3: listen using unix_listen_fd */
    if( listen(unix_listen_fd, MY_BACKLOG) == -1){
        log_file_write_fatal_error("%s: listen", __func__);
        fprintf(stderr, "err: %s: listen\n", __func__);
    }

    /* step4 create epoll fd */
    if( (ep_fd = epoll_create1(0)) == -1){
        log_file_write_fatal_error("%s: epoll_create1", __func__);
        fprintf(stderr, "err: %s: epoll_create1\n", __func__);
    }
    
    /* step5 add check_heartbeat_timer_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    if( ENABLE_EXTERNAL_APP_HEARTBEAT_PERIODIC_CHECK ){
        if(set_timer_fd_for_checking_app_hearbeat()){
            log_file_write_fatal_error("err: %s: set_timer_fd_for_checking_app_hearbeat() failed\n", __func__);
            fprintf(stderr, "err: %s: set_timer_fd_for_checking_app_hearbeat() failed\n", __func__);
        }
    } 

    /* step6 add unix_listen_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = unix_listen_fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, unix_listen_fd, &ev)){
        log_file_write_fatal_error("%s: epoll_ctl", __func__);
        fprintf(stderr, "err: %s: epoll_ctl\n", __func__);
    }
    
    struct epoll_event ev_arr[EPOLL_MAX_EVENTS];    
    int client_fd;       //temporary store client that we are communicating with.
    int num_ev_ready;    //number of events which are ready, returned by epoll_wait()
    log_file_write("%s: is ready to enter main loop\n", __func__);
    fprintf(stdout, "%s: is ready to enter main loop\n", __func__);

    /* step7 start polling connection using epoll_wait */
    while(1){ 
        /* epoll_wait():
         * arg1: fd for epoll 
         * arg2: array for storing events
         * arg3: max events number for one epoll_wait()
         * arg4: max wait-time */ 
		num_ev_ready = epoll_wait(ep_fd, ev_arr, EPOLL_MAX_EVENTS, -1);
        for(uint32_t i = 0; i < num_ev_ready; ++i){
            //ignore not EPOLLIN
            if(!(ev_arr[i].events & EPOLLIN)){
				continue;   
            }
            
            if (ev_arr[i].data.fd == check_heartbeat_timer_fd){   /* timer expired */
                unsigned long long junk_var;  //used to clear readable flag of timer_fd
                (void)read(check_heartbeat_timer_fd, &junk_var, sizeof(junk_var));
                
                int ret = check_all_external_app_heartbeats();
                fprintf(stdout, "[EAP msg] check_all_external_app_heartbeats() ret %d\n",  ret);
                log_file_write("[EAP msg] check_all_external_app_heartbeats() ret %d\n", ret);
            }
            else if (ev_arr[i].data.fd == unix_listen_fd){    /* new client want to link */

                if( (client_fd = accept(unix_listen_fd, NULL, NULL)) == -1){
                    log_file_write_fatal_error("[EAP msg] err: %s: accept()", __func__);
                    fprintf(stderr, "[EAP msg] err: %s: accept\n", __func__);
                }

                //log_file_write("new client_fd to external_app_proxy: %d\n", client_fd);
                //fprintf(stdout, "new client_fd to external_app_proxy: %d\n", client_fd);

                /* add this new client to epoll */
                struct epoll_event new_ev;
                new_ev.events = EPOLLIN;
                new_ev.data.fd = client_fd;
				epoll_ctl(ep_fd, EPOLL_CTL_ADD, client_fd, &new_ev);

                handle_new_client_accepted(client_fd);
			}
            else{   /* existed client */
                    
                /* packet-sent from client, or socket-unlinked by client */
                /* if read() from client_fd return 0 byte, meaning it unlink */
     			client_fd = ev_arr[i].data.fd;

                handle_remote_client_request(client_fd);			
			}

        } //for( i < num_ev_ready )
    
    } //while (1)

    log_file_write_fatal_error("[EAP msg] err: %s: thread unexpected exit", __func__);
    fprintf(stderr, "[EAP msg] err: %s: thread unexpected exit\n", __func__);
}

int32_t recv_packet_from_unix_sk_fd(int socket_fd, void* packet_p, size_t packet_size)
{    
    int ret = recv(socket_fd, packet_p, packet_size, 0);

    if( ret == 0 ){  /* meaning that remote client close the fd (maybe due to crash) */
        return EAL_ERR_SOCKET_DISCONNECT;
    }
    else if ( ret  < 0 ){
        char* errno_str = strerror(errno);
        if( !errno_str ) 
            errno_str = "undefined/zero errno";
        fprintf(stderr, "%s: recv() ret -1, strerror() shows: %s\n", __func__, errno_str);
        log_file_write("%s: recv() ret -1, strerror() shows: %s\n", __func__, errno_str);
        return EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t send_packet_to_unix_sk_fd(int socket_fd, void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    if( (ret = send(socket_fd, packet_p, packet_size, MSG_NOSIGNAL)) == -1){
        current_errno = errno;
        char* errno_str = strerror(current_errno);
        if( !errno_str ) 
            errno_str = "undefined/zero errno";
        fprintf(stderr, "%s: send() ret -1, strerror() shows: %s\n", __func__, errno_str);
        log_file_write("%s: send() ret -1, strerror() shows: %s\n", __func__, errno_str);
        if( errno == -EPIPE){
            /* meaning that remote client close the fd (maybe due to crash) */
            return EAL_ERR_SOCKET_DISCONNECT;
        }
        return EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}


