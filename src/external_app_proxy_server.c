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
#include "external_app_proxy_callback_msg_forward.h"
#include "external_app_proxy_server.h"

/* WARN: make sure the path MY_UNIX_SOCKET_PATH correctly locate the file for bind() 
 * client application (i.e., external app) will use EAL (external application library) 
 * to try to register to the middleware (EAP).
 * And the EAL internal codes will and should use the exact same file as the middleware */
//#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
#define MY_UNIX_SOCKET_PATH "../RSU_Controller_master/config/my_unix_socket_file_for_eap"
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

int32_t eap_recv_packet_from_unix_sk(int socket_fd, void* packet_p, size_t packet_size);
int32_t eap_send_packet_to_unix_sk(int socket_fd, void* packet_p, size_t packet_size);
int32_t eap_simple_send_ack(int client_fd, void* ack_p, size_t ack_size, int ret_if_suc);

/* the cbmsg_forward_fp_arr[] will be used by "despather", "indirectly" 
 * NOTICE, if you add new callback, you NEED to update 
 * this function: inner_set_external_app_callback_by_mask() */
static inline __attribute__((always_inline)) 
int inner_set_external_app_callback_by_mask(app_obj_t* app_obj_p, uint64_t mask)
{
    if( !mask ){
        return 0;
    }
    if( !app_obj_p ){
        return -1;
    }

    if( mask & ( 0x1 << EVENT_OBU_PACKET_RX ) ){
        app_obj_p->on_OBU_packet_rx = cbmsg_forward_fp_arr[EVENT_OBU_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_OBU_PACKET_TX ) ){
        app_obj_p->on_OBU_packet_tx = cbmsg_forward_fp_arr[EVENT_OBU_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_RSU_PACKET_RX ) ){
        app_obj_p->on_RSU_packet_rx = cbmsg_forward_fp_arr[EVENT_RSU_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_RSU_PACKET_TX ) ){
        app_obj_p->on_RSU_packet_tx = cbmsg_forward_fp_arr[EVENT_RSU_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_CLOUD_PACKET_RX ) ){
        app_obj_p->on_cloud_packet_rx = cbmsg_forward_fp_arr[EVENT_CLOUD_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_CLOUD_PACKET_TX ) ){
        app_obj_p->on_cloud_packet_tx = cbmsg_forward_fp_arr[EVENT_CLOUD_PACKET_TX];
    }
    if( mask & ( 0x1 << EVENT_TRAFFIC_SIGNAL_COMMAND_TX ) ){
        app_obj_p->on_traffic_signal_command_tx = cbmsg_forward_fp_arr[EVENT_TRAFFIC_SIGNAL_COMMAND_TX];
    }
    if( mask & ( 0x1 << EVENT_CAMERA_PACKET_RX ) ){
        app_obj_p->on_camera_packet_rx = cbmsg_forward_fp_arr[EVENT_CAMERA_PACKET_RX];
    }
    if( mask & ( 0x1 << EVENT_REGISTRATION ) ){
        /* since for external application,
           the on_registration callback will be directly called at client side 
           we just IGNORE the on_registration callback at server side 
        */
        ; //do nothing in current version
    }
    if( mask & ( 0x1 << EVENT_MIDDLEWARE_RESTART ) ){
        app_obj_p->on_middleware_restart = cbmsg_forward_fp_arr[EVENT_MIDDLEWARE_RESTART];
    }
    return 0;
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

    check_heartbeat_its.it_interval.tv_sec = HEARTBEAT_MONITOR_PERIOD_S;
    check_heartbeat_its.it_interval.tv_nsec = 0;
    check_heartbeat_its.it_value.tv_sec = HEARTBEAT_MONITOR_START_OFFSET_S;
    check_heartbeat_its.it_value.tv_nsec = 0;
    
    int ret = timerfd_settime(check_heartbeat_timer_fd, 0, &check_heartbeat_its, NULL);
    if ( ret < 0) {
        fprintf(stderr, 
            "set_timer_fd_for_checking_app_hearbeat()"
            "timerfd_settime with check_heartbeat_its ret %d\n", ret);

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
        fprintf(stderr, 
            "set_timer_fd_for_checking_app_hearbeat()"
            "epoll_ctl(EPOLL_CTL_ADD) ret %d\n", ret);

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
    /* After a socket fd being closed, 
     * that socket fd will automatically de-register itself from epoll,
     * (as long as the middleware is single-process, and that the fd is only referenced by EAP),
     * so currently we do nothing in this function.
     * If in later linux version, the behavior of epoll changes,
     * this function might need to be implemented */
    return 0;
}

static inline  __attribute__((always_inline))  
int unlink_an_external_app(app_obj_t *app_obj_p)
{
    int ret;
    ret = remove_both_channels_from_proxy_epoll(app_obj_p);
    if(ret){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf(stderr,
            "[EAP msg] err: %s call: remove_both_channels_from_proxy_epoll() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write(
            "[EAP msg] err: %s call: remove_both_channels_from_proxy_epoll() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);
        #endif
    }

    ret = inner_close_both_channels_of_an_external_app(app_obj_p);
    if(ret){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf(stderr,
            "[EAP msg] err: %s call: inner_close_both_channels_of_an_external_app() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write(
            "[EAP msg] err: %s call: inner_close_both_channels_of_an_external_app() " 
            "for appID:%d, ret = %d\n", __func__, app_obj_p->id, ret);
        #endif

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
        goto unlock_ret;
    }
    /* traverse each node */
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
            if ( record_diff > HEARTBEAT_MONITOR_ALLOWED_THERSHHOLD ) {
                unlink_an_external_app(current);

                #if ENABLE_PRINTING_EAP_DETECTED_ERR
                fprintf(stderr,
                    "[EAP msg] %s call: unlink_an_external_app() for appID:%d, ret = %d\n",
                    __func__, current->id, ret);
                #endif
                #if ENABLE_LOGGING_EAP_DETECTED_ERR
                log_file_write(
                    "[EAP msg] %s call: unlink_an_external_app() for appID:%d, ret = %d\n",
                    __func__, current->id, ret);
                #endif
            }
        }
        current = current->next;
    }

    pthread_mutex_unlock(&mutex_app_list); 

unlock_ret: 
    proxy_cur_heartbeat += 1; 
    return ret;
}

/* this function will use api_wrapper_fp_arr, which will send unix packet to external app */
static inline __attribute__((always_inline)) 
int inner_handle_request_by_api_id(int client_fd, uint32_t api_id)
{   
    if ( client_fd == 0 ){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf(stderr, "err: inner_handle_request_by_api_id: client_fd == 0\n");
        #endif
        return -1;
    }
    if ( api_id <= API_ID_OF(app_remote_register) || api_id >= NUM_OF_API_ID_DEFININITION ){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf(stderr, "err: inner_handle_request_by_api_id: bad api_id == %d\n", api_id);
        #endif
        return -2;
    }

    /* call the related wrapper function by its api_id */
    int ret = (*api_wrapper_fp_arr[api_id])(client_fd);
    return ret;
}

static inline __attribute__((always_inline)) 
int inner_handle_heartbeat_from_app(int client_fd, packet_header_to_proxy_t *header_p)
{   
    int ret = update_external_app_heartbeat_by_appID(header_p->appID);
    if(ret){
        ret = -EAL_ERR_IN_MIDDLEWARE_HEARTBEAT_UPDATE_FOR_APP;
    }
    return ret;
}

uint8_t get_current_eap_heartbeat_rc()
{
    return proxy_cur_heartbeat;
}

int handle_new_client_registration(int client_fd)
{
    /* WARNNING!!
     * make sure the packet you send-to/recv-from proxy client 
     * "MATCH" the send-to/recv-from action 
     *  in app_remote_register() in external_app_proxy_client.c
     */

    int ret;

    //prepare ack packet
    packet_header_from_proxy_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.packet_type = EA_PACKET_TYPE_ACK;

    //read header
    packet_header_to_proxy_t header;
    ret = eap_recv_packet_from_unix_sk( client_fd, &header, sizeof(header));
    if( ret != 0 ){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf( stderr, "line %d, %s: recv header ret: %d\n",
                 __LINE__, __func__ , ret);
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write("line %d, %s: recv header ret: %d\n",
                 __LINE__, __func__ , ret);
        #endif
        return ret;
    }

    /* header error-check */
    if( header.packet_type != EA_PACKET_TYPE_REGISTER_TOP
        && header.packet_type != EA_PACKET_TYPE_REGISTER_BOT)
    {   
        ack.ret_val = -EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER;

        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf( stderr, "[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER");
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write("[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER");
        #endif

        return eap_simple_send_ack(client_fd, &ack, sizeof(ack), 1); //fail at check 1
    }
    else if( header.api_id != API_ID_OF(app_remote_register) )
    {
        ack.ret_val = -EAL_ERR_BAD_API_ID_BEFORE_REGISTER;

        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf( stderr, "[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_API_ID_BEFORE_REGISTER");
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write("[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_API_ID_BEFORE_REGISTER");
        #endif

        return eap_simple_send_ack(client_fd, &ack, sizeof(ack), 2); //fail at check 2
    }

    /* then read payload */
    if( header.packet_type == EA_PACKET_TYPE_REGISTER_TOP ){
        
        /* new registration or re-registration */
        struct {
            char name[APP_NAME_MAX_LEN];
            uint8_t id;
            uint8_t priority;
            uint8_t dontSend2TC;
            uint64_t callback_register_mask;
            pid_t pid;
        } payload_top;

        if( (ret = eap_recv_packet_from_unix_sk( client_fd, &payload_top, sizeof(payload_top)) ) != 0){
            #if ENABLE_PRINTING_EAP_DETECTED_ERR
            fprintf( stderr, "line %d, %s: recv payload_top ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
            #if ENABLE_LOGGING_EAP_DETECTED_ERR
            log_file_write("line %d, %s: recv payload_top ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
            return ret;
        }

        app_obj_t* in_MW_app_p;  //MW: middleware
        in_MW_app_p = get_app_obj_by_appID(payload_top.id);
        if( !in_MW_app_p ){  /* brand-new app registration */

            Malloc(in_MW_app_p, sizeof(app_obj_t), "in_MW_app_p");
            Malloc(in_MW_app_p->ea_info_p, sizeof(ea_info_t), "in_MW_app_p->ea_info_p");
            
            strncpy( in_MW_app_p->name, payload_top.name, APP_NAME_MAX_LEN);
            in_MW_app_p->dontSend2TC = payload_top.dontSend2TC;
            in_MW_app_p->id = payload_top.id;
            in_MW_app_p->priority = payload_top.priority;
            inner_set_external_app_callback_by_mask(in_MW_app_p, payload_top.callback_register_mask);
            
            ret = app_register(in_MW_app_p);  /* using existed function in application_registration.c */
            if(ret != 0){
                ack.ret_val = -EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT;

                #if ENABLE_PRINTING_EAP_DETECTED_ERR
                fprintf( stderr, "[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                               ack.ret_val, "EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT" );
                #endif
                #if ENABLE_LOGGING_EAP_DETECTED_ERR
                log_file_write("[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                               ack.ret_val, "EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT" );
                #endif

                return eap_simple_send_ack(client_fd, &ack, sizeof(ack), 3); //fail at check 3
            }
        }

        /*this will set I-channel to client_fd and N-channel to 0*/
        ret = reset_external_app_two_new_sk_fds(in_MW_app_p, client_fd);  
        in_MW_app_p->ea_info_p->heartbeat_rc = proxy_cur_heartbeat;
        in_MW_app_p->ea_info_p->pid = payload_top.pid;
        
        ack.ret_val = EAL_ERR_OK;
        /*we always log successful registration*/
        log_file_write("[EAP msg] for the registration top, ack.ret_val is:%d ->%s\n", 
                               ack.ret_val, "EAL_ERR_OK" );
        return eap_simple_send_ack(client_fd, &ack, sizeof(ack), 0);
    }
    else{  /* I.e., header.packet_type == EA_PACKET_TYPE_REGISTER_BOT */

        //prepare ack packet
        packet_header_from_proxy_t ntf_ack;
        ntf_ack.packet_type = EA_PACKET_TYPE_ACK;

        struct {
            uint8_t appID;
            pid_t pid;
        } payload_bot;

        if( (ret = eap_recv_packet_from_unix_sk( client_fd, &payload_bot, sizeof(payload_bot)) ) != 0){
            
            #if ENABLE_PRINTING_EAP_DETECTED_ERR
            fprintf( stderr, "line %d, %s: recv payload_bot ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
            #if ENABLE_LOGGING_EAP_DETECTED_ERR
            log_file_write("line %d, %s: recv payload_bot ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
            
            return ret;
        }

        app_obj_t* in_MW_app_p = get_app_obj_by_appID(payload_bot.appID);
        if( !in_MW_app_p ){
            ntf_ack.ret_val = -EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE;
        }
        else if( in_MW_app_p->ea_info_p->pid != payload_bot.pid ) {
            /* NOTIFY PACKET should not come from another process 
             * different from the one who link the first channel */
            ntf_ack.ret_val = -EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE;
        }
        else{
            ntf_ack.ret_val = EAL_ERR_OK;
            update_external_app_notify_fd(in_MW_app_p, client_fd);
            in_MW_app_p->ea_info_p->heartbeat_rc = proxy_cur_heartbeat;
        }

        ret = eap_simple_send_ack(client_fd, &ntf_ack, sizeof(ntf_ack), 0);
        log_file_write("[EAP msg] for the registration bot, ntf_ack.ret_val is:%d ->%s\n", 
                         ntf_ack.ret_val, get_str_by_err_code(ntf_ack.ret_val) );
        if( ret ){
            return ret;
        }

        if(ntf_ack.ret_val == EAL_ERR_OK){
            
            struct {
                uint32_t RSU_id;
                double RSU_lat;
                double RSU_lon;
                char RSU_name[RSU_NAME_MAX_LEN];
                double RSU_elev;
                uint32_t RSU_region;
            } success_ack_payload;

            success_ack_payload.RSU_id = config.RSU_id;
            success_ack_payload.RSU_lat = config.RSU_lat;
            success_ack_payload.RSU_lon = config.RSU_lon;
            success_ack_payload.RSU_elev = config.RSU_elev;
            success_ack_payload.RSU_region = config.RSU_region;
            strncpy(success_ack_payload.RSU_name, config.RSU_name, RSU_NAME_MAX_LEN);
            
            ret = eap_send_packet_to_unix_sk(client_fd, &success_ack_payload, sizeof(success_ack_payload));
            if( ret ){
                #if ENABLE_PRINTING_EAP_DETECTED_ERR
                fprintf( stderr, "line %d, %s: send success_ack_payload ret: %d\n",
                        __LINE__, __func__ , ret);
                #endif
                #if ENABLE_LOGGING_EAP_DETECTED_ERR
                log_file_write("line %d, %s: send success_ack_payload ret: %d\n",
                        __LINE__, __func__ , ret);
                #endif
                
                return ret;
            }
        }

        print_and_log_external_app_register_msg(in_MW_app_p);
    }
    return ret;
}

int handle_registered_client_packet(int client_fd)
{
    int ret;

    //prepare ack packet
    packet_header_from_proxy_t ack;
    ack.packet_type = EA_PACKET_TYPE_ACK;

    packet_header_to_proxy_t header;
    ret = eap_recv_packet_from_unix_sk( client_fd, &header, sizeof(header));
    if (ret != 0) {
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf( stderr, "line %d, %s: recv header ret: %d\n",
                __LINE__, __func__ , ret);
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write("line %d, %s: recv header ret: %d\n",
                __LINE__, __func__ , ret);
        #endif

        goto err_handling;
    }

    /* header error-check */
    if ( header.packet_type != EA_PACKET_TYPE_REQ 
         && header.packet_type != EA_PACKET_TYPE_REPORT) 
    {   
        ack.ret_val = -EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE;

        #if ENABLE_PRINTING_EAP_DETECTED_ERR
        fprintf( stderr, "[EAP msg] for the ea request, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE" );
        #endif
        #if ENABLE_LOGGING_EAP_DETECTED_ERR
        log_file_write("[EAP msg] for the ea request, ack.ret_val is:%d ->%s\n", 
                       ack.ret_val, "EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE" );
        #endif

        ret = eap_simple_send_ack(client_fd, &ack, sizeof(ack), 1);
        if(ret !=0 ){
            #if ENABLE_PRINTING_EAP_DETECTED_ERR
            fprintf( stderr, "line %d, %s: send ack ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
            #if ENABLE_LOGGING_EAP_DETECTED_ERR
            log_file_write("line %d, %s: send ack ret: %d\n",
                    __LINE__, __func__ , ret);
            #endif
        }
        goto err_handling;
    }
    
    if ( header.packet_type == EA_PACKET_TYPE_REPORT ) {
        #if ENABLE_PRINTING_EAP_HEARTBEAT_RECEIVING
            printf("[EAP msg] get heartbeat packet from appID:%u\n", 
                    header.appID);
        #endif
        #if ENABLE_LOGGING_EAP_HEARTBEAT_RECEIVING
        log_file_write("[EAP msg] get heartbeat packet from appID:%u\n", 
                        header.appID);
        #endif

        ret = inner_handle_heartbeat_from_app(client_fd, &header);

        if( ret == -EAL_ERR_IN_MIDDLEWARE_HEARTBEAT_UPDATE_FOR_APP ){
            #if ENABLE_PRINTING_EAP_DETECTED_ERR
                printf("[EAP msg] heartbeat record update failed for appID:%u\n", 
                        header.appID);
            #endif
            #if ENABLE_LOGGING_EAP_DETECTED_ERR
            log_file_write("[EAP msg] heartbeat record update failed for appID:%u\n", 
                            header.appID);
            #endif
        }
    }
    else{   /* i.e., header.packet_type == EA_PACKET_TYPE_REQ */
        #if ENABLE_PRINTING_EAP_REQUEST_HANDLING
            printf("[EAP msg] get request packet from appID:%u, api_id:%d ->%s() \n", 
                    header.appID, header.api_id, api_id_str_arr[header.api_id] );
        #endif
        #if ENABLE_LOGGING_EAP_REQUEST_HANDLING
        log_file_write("[EAP msg] get request packet from appID:%u, api_id:%d ->%s() \n", 
                        header.appID, header.api_id, api_id_str_arr[header.api_id] );
        #endif

        ret = inner_handle_request_by_api_id(client_fd, header.api_id);

        if( ret ){
            #if ENABLE_PRINTING_EAP_DETECTED_ERR
                printf("[EAP msg] handle request failed with ret: %d\n", ret); 
            #endif
            #if ENABLE_LOGGING_EAP_DETECTED_ERR
            log_file_write("[EAP msg] handle request failed with ret: %d\n", ret);
            #endif
        }
    }

err_handling:

    // special handling for EAL_ERR_SOCKET_DISCONNECT
    if( ret == -EAL_ERR_SOCKET_DISCONNECT ){
        app_obj_t *app_obj_p = get_app_obj_by_unix_socket_fd(client_fd);
        if(app_obj_p){
            pthread_mutex_lock(&mutex_app_list); 
            unlink_an_external_app(app_obj_p);
            pthread_mutex_unlock(&mutex_app_list); 
        }
        else{
            /* this case should never happend */
            fprintf(stderr, 
                "[EAP msg] err: get_app_obj_by_unix_socket_fd() "
                "cannot find matching app with client_fd: %d\n", 
                client_fd );
            log_file_write(
                "[EAP msg] err: get_app_obj_by_unix_socket_fd() "
                "cannot find matching app with client_fd: %d\n", 
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
        log_file_write("[EAP MSG] warn. %s: MY_UNIX_SOCKET_PATH is already exist\n" 
                       "will call unlink() before re-link\n", __func__);
        fprintf(stderr, "[EAP MSG] warn. %s: MY_UNIX_SOCKET_PATH is already exist\n" 
                        "will call unlink() before re-link\n", __func__);
        if (unlink (MY_UNIX_SOCKET_PATH) == -1){
	        log_file_write_fatal_error("%s: unlink", __func__);
            fprintf(stderr, "err: %s: unlink\n", __func__);
        }
    }

    /* step1: create unix domain socket fd: unix_listen_fd */
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
    if( ENABLE_PERIODIC_HEARTBEAT_MONITOR ){
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
    log_file_write("[EAP MSG] %s: is ready to enter main loop\n", __func__);

    /* step7 start polling connection using epoll_wait */
    while(1){ 
        /* epoll_wait():
         * arg1: fd for epoll 
         * arg2: array for storing events
         * arg3: max events number for one epoll_wait()
         * arg4: max wait-time 
         * */ 
		num_ev_ready = epoll_wait(ep_fd, ev_arr, EPOLL_MAX_EVENTS, -1);
        for(uint32_t i = 0; i < num_ev_ready; ++i){
            //ignore not EPOLLIN
            if(!(ev_arr[i].events & EPOLLIN)){
				continue;   
            }
            
            if (ev_arr[i].data.fd == check_heartbeat_timer_fd){   /* CASE 1: timer expired */
                unsigned long long junk_var;  //used to clear readable flag of timer_fd
                (void)read(check_heartbeat_timer_fd, &junk_var, sizeof(junk_var));
                int ret = check_all_external_app_heartbeats();

                #if ENABLE_PRINTING_EAP_DETECTED_ERR
                    if(ret) 
                        fprintf(stderr, "[EAP msg] check_all_external_app_heartbeats() ret %d\n",  ret);
                #endif
                log_file_write("[EAP msg] check_all_external_app_heartbeats() ret %d\n", ret);
            }
            else if (ev_arr[i].data.fd == unix_listen_fd){    /* CASE 2: new client want to link */

                if( (client_fd = accept(unix_listen_fd, NULL, NULL)) == -1){
                    fprintf(stderr, "[EAP msg] err: %s: accept, WHY??\n", __func__);
                    log_file_write("[EAP msg] err: %s: accept, WHY??\n", __func__);
                    continue;
                }

                /* enter registration process, notice that each new client will link 2 times to establish 2 channel */
                if(handle_new_client_registration(client_fd) == 0){
                    /* add this new client to epoll */
                    struct epoll_event new_ev;
                    new_ev.events = EPOLLIN;
                    new_ev.data.fd = client_fd;
                    epoll_ctl(ep_fd, EPOLL_CTL_ADD, client_fd, &new_ev);
                }
			}
            else{   /* CASE 3: already registered clients */
                /* packet-sent from client, or socket-unlinked by client */
                /* if read() from client_fd return 0 byte, meaning it is a unlink msg */
     			client_fd = ev_arr[i].data.fd;
                handle_registered_client_packet(client_fd);			
			}

        } //for( i < num_ev_ready )
    
    } //while (1)

    fprintf(stderr, "[EAP msg] err: %s: thread unexpected exit\n", __func__);
    log_file_write_fatal_error("[EAP msg] err: %s: thread unexpected exit", __func__);
}

int32_t eap_recv_packet_from_unix_sk(int socket_fd, void* packet_p, size_t packet_size)
{    
    int ret = recv(socket_fd, packet_p, packet_size, 0);

    if( ret == 0 ){  /* meaning that remote client close the fd (maybe due to crash) */
        return -EAL_ERR_SOCKET_DISCONNECT;
    }
    else if ( ret  < 0 ){
        char* errno_str = strerror(errno);
        if( !errno_str ){
            errno_str = "undefined/zero errno";
        }
        #if ENABLE_PRINTING_EAP_INNER_SOCKET_ERR
        fprintf(stderr, "[EAP MSG] %s: recv() ret -1, strerror() shows: %s\n", __func__, errno_str);
        #endif
        #if ENABLE_LOGGING_EAP_INNER_SOCKET_ERR
        log_file_write("[EAP MSG] %s: recv() ret -1, strerror() shows: %s\n", __func__, errno_str);
        #endif

        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t eap_send_packet_to_unix_sk(int socket_fd, void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    if( (ret = send(socket_fd, packet_p, packet_size, MSG_NOSIGNAL)) == -1){
        current_errno = errno;
        char* errno_str = strerror(current_errno);
        if( !errno_str ){
            errno_str = "undefined/zero errno";
        }
        #if ENABLE_PRINTING_EAP_INNER_SOCKET_ERR
        fprintf(stderr, "[EAP MSG] %s: send() ret -1, strerror() shows: %s\n", __func__, errno_str);
        #endif
        #if ENABLE_LOGGING_EAP_INNER_SOCKET_ERR
        log_file_write("[EAP MSG] %s: send() ret -1, strerror() shows: %s\n", __func__, errno_str);
        #endif

        if( errno == -EPIPE){
            /* meaning that remote client close the fd (maybe due to crash) */
            return -EAL_ERR_SOCKET_DISCONNECT;
        }
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t eap_simple_send_ack(int client_fd, void* ack_p, size_t ack_size, int ret_if_suc){
    int ret = eap_send_packet_to_unix_sk( client_fd, ack_p, ack_size);
    if( ret != 0){
        #if ENABLE_PRINTING_EAP_DETECTED_ERR
            fprintf(stderr, "[EAP msg] %d: eap_send_packet_to_unix_sk to fd: %d ret: %d\n",
                              __LINE__, client_fd, ret);
        #endif
    }
    else{
        ret = ret_if_suc;
    }
    return ret;
}

