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
#include "external_app_proxy_api_wrapper.h"
#include "external_app_proxy_callback_wrapper.h"
#include "external_app_proxy_server.h"

#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
#define EPOLL_MAX_EVENTS 64

/* The backlog argument defines the maximum length to which the
 * queue of pending connections for sockfd may grow. 
 * Reference: https://man7.org/linux/man-pages/man2/listen.2.html */
#define MY_BACKLOG 20

static int current_errno;
static int unix_listen_fd;  //only one, used for accepting new client
static int ep_fd;           //fd for epoll
static uint8_t proxy_cur_heartbeat;

int external_app_proxy_notify_callback(void* info);
uint8_t get_current_eap_heartbeat_rc();
void *external_app_proxy_handler();
int32_t recv_from_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);
int32_t send_to_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);

app_obj_t* find_duplicate_id_app_obj( uint8_t req_id );
int handle_new_client_fd_accepted(int client_fd);
int handle_remote_client_request(int client_fd);
static inline int inner_handle_app_register( app_obj_t* app_p, void *payload_p);
static inline int inner_handle_heartbeat_from_app(int client_fd);
static inline int inner_handle_request_by_api_id(int client_fd, uint32_t api_id);
uint8_t get_current_eap_heartbeat_rc();
void increase_eap_heartbeat_rc();
int check_all_external_app_heartbeat();

/* the callback_wrapper_fp_arr[] will be used by "despather", "indirectly" */
/* NOTICE, if you add new callback, you NEED to update this function */
static inline void inner_set_external_app_callback_by_mask(app_obj_t* app_obj_p, uint64_t mask)
{
    if( mask |= ( 0x1 << EVENT_OBU_PACKET_RX ) ){
        app_obj_p->on_OBU_packet_rx = callback_wrapper_fp_arr[EVENT_OBU_PACKET_RX];
    }
    if( mask |= ( 0x1 << EVENT_OBU_PACKET_TX ) ){
        app_obj_p->on_OBU_packet_tx = callback_wrapper_fp_arr[EVENT_OBU_PACKET_TX];
    }
    if( mask |= ( 0x1 << EVENT_RSU_PACKET_RX ) ){
        app_obj_p->on_RSU_packet_rx = callback_wrapper_fp_arr[EVENT_RSU_PACKET_RX];
    }
    if( mask |= ( 0x1 << EVENT_RSU_PACKET_TX ) ){
        app_obj_p->on_RSU_packet_tx = callback_wrapper_fp_arr[EVENT_RSU_PACKET_TX];
    }
    if( mask |= ( 0x1 << EVENT_CLOUD_PACKET_RX ) ){
        app_obj_p->on_cloud_packet_rx = callback_wrapper_fp_arr[EVENT_CLOUD_PACKET_RX];
    }
    if( mask |= ( 0x1 << EVENT_CLOUD_PACKET_TX ) ){
        app_obj_p->on_cloud_packet_tx = callback_wrapper_fp_arr[EVENT_CLOUD_PACKET_TX];
    }
    if( mask |= ( 0x1 << EVENT_TRAFFIC_SIGNAL_COMMAND_TX ) ){
        app_obj_p->on_traffic_signal_command_tx = callback_wrapper_fp_arr[EVENT_TRAFFIC_SIGNAL_COMMAND_TX];
    }
    if( mask |= ( 0x1 << EVENT_CAMERA_PACKET_RX ) ){
        app_obj_p->on_camera_packet_rx = callback_wrapper_fp_arr[EVENT_CAMERA_PACKET_RX];
    }
    if( mask |= ( 0x1 << EVENT_REGISTRATION ) ){
        /* since for external application,
           the on_registration callback will be directly called at client side 
           we just IGNORE the on_registration callback at server side */
        //app_obj_p->on_registration = callback_wrapper_fp_arr[EVENT_REGISTRATION];
        ; //do nothing in current version
    }
    if( mask |= ( 0x1 << EVENT_MIDDLEWARE_RESTART ) ){
        app_obj_p->on_middleware_restart = callback_wrapper_fp_arr[EVENT_MIDDLEWARE_RESTART];
    }
}

uint8_t get_current_eap_heartbeat_rc()
{
    return proxy_cur_heartbeat;
}

void increase_eap_heartbeat_rc()
{
    /* unsigned type will auto round up */
    /* since we compare heartbeat with a threshold much larger than 1,
     * I think it's ok to not protect it by mutex */
    proxy_cur_heartbeat += 1;   
}

/* the external_app_proxy server "main thread" */
void *external_app_proxy_handler()
{   
    /* step0: create a unix domain socket with MY_UNIX_SOCKET_PATH 
     * unlink, if socket already exists */
    struct stat statbuf;
    if( stat (MY_UNIX_SOCKET_PATH, &statbuf) == 0) {
        log_file_write("external_app_proxy_handler: MY_UNIX_SOCKET_PATH is already exist");
        fprintf(stdout, "warn: external_app_proxy_handler: MY_UNIX_SOCKET_PATH is already exist\n");
        if (unlink (MY_UNIX_SOCKET_PATH) == -1){
	        log_file_write_fatal_error("external_app_proxy_handler: unlink");
            fprintf(stderr, "err: external_app_proxy_handler: unlink\n");
        }
    }

    /* step1: create a unix domain socket */
    if( (unix_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
	    log_file_write_fatal_error("external_app_proxy_handler: socket");
        fprintf(stderr, "err: external_app_proxy_handler: socket\n");
    }

    /* step2: bind the socket to MY_UNIX_SOCKET_PATH */
    /* sun_path in a char array with size = 108, be care of your "sk_addr.sun_path" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    if( bind(unix_listen_fd, (const struct sockaddr *) &sk_addr, sizeof (struct sockaddr_un)) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: bind");
        fprintf(stderr, "err: external_app_proxy_handler: bind\n");
    }

    /* step3: listen using unix_listen_fd */
    if( listen(unix_listen_fd, MY_BACKLOG) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: listen");
        fprintf(stderr, "err: external_app_proxy_handler: listen\n");
    }

    /* step4 create epoll fd */
    if( (ep_fd = epoll_create1(0)) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: epoll_create1");
        fprintf(stderr, "err: external_app_proxy_handler: epoll_create1\n");
    }

    /* step5 add unix_listen_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = unix_listen_fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, unix_listen_fd, &ev)){
        log_file_write_fatal_error("external_app_proxy_handler: epoll_ctl");
        fprintf(stderr, "err: external_app_proxy_handler: epoll_ctl\n");
    }
    
    struct epoll_event ev_arr[EPOLL_MAX_EVENTS];    
    int client_fd;       //temporary store client that we are communicating with.
    int num_ev_ready;    //number of events which are ready, returned by epoll_wait()
    log_file_write("external_app_proxy_handler: is ready to enter main loop");
    fprintf(stdout, "external_app_proxy_handler: is ready to enter main loop\n");

    /* step6 start polling connection using epoll_wait */
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
            
            if (ev_arr[i].data.fd == unix_listen_fd){    /* new client want to link */

                if( (client_fd = accept(unix_listen_fd, NULL, NULL)) == -1){
                    log_file_write_fatal_error("external_app_proxy_handler: accept");
                    fprintf(stderr, "err: external_app_proxy_handler: accept\n");
                }

                log_file_write("new client_fd to external_app_proxy: %d\n", client_fd);
                fprintf(stdout, "new client_fd to external_app_proxy: %d\n", client_fd);

                /* add this new client to epoll */
                struct epoll_event new_ev;
                new_ev.events = EPOLLIN;
                new_ev.data.fd = client_fd;
				epoll_ctl(ep_fd, EPOLL_CTL_ADD, client_fd, &new_ev);

                handle_new_client_fd_accepted(client_fd);
			}
            else{   /* existed client */
                    
                /* packet-sent from client, or socket-unlinked by client */
                /* if read() from client_fd return 0 byte, meaning it unlink */
     			client_fd = ev_arr[i].data.fd;

                handle_remote_client_request(client_fd);			
			}

        } //for( i < num_ev_ready )
    
    } //while (1)

    log_file_write_fatal_error("external_app_proxy_handler: thread unexpected exit");
    fprintf(stderr, "err: external_app_proxy_handler: thread unexpected exit\n");
}

int handle_remote_client_request(int client_fd)
{
    int ret;
    packet_to_proxy_header_t header;
    ack_from_proxy_header_t ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    ret = read_from_unix_socket_fd( client_fd, &header, sizeof(header));
    if (ret != 0) {
        return -1;
    }

    /* header error-check */
    if ( header.packet_type != EA_PACKET_TYPE_REQ 
         && header.packet_type != EA_PACKET_TYPE_HEARTBEAT) 
    {
        ack_packet.ret_val = EA_ERR_BAD_PACKET_TYPE_FROM_INTERACT_CHANNEL;
        ret = send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return ret;
    }

    if ( header.packet_type == EA_PACKET_TYPE_HEARTBEAT ) {
        log_file_write("ea_proxy get heartbeat packet from app ID:%u", header.appID);
        ret = inner_handle_heartbeat_from_app(client_fd);
    }
    else{   /* i.e., header.packet_type == EA_PACKET_TYPE_REQ */
        log_file_write("ea_proxy get request packet with api id %d:%s from app ID:%u", 
                        header.api_id, api_id_str_arr[header.api_id], header.appID);
        ret = inner_handle_request_by_api_id(client_fd, header.api_id);
    }
    return ret;
}

/* this function will use api_wrapper_fp_arr, which will send unix packet to external app */
static inline int inner_handle_request_by_api_id(int client_fd, uint32_t api_id)
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
    
    if( ret == EA_ERR_SOCKET_DISCONNECT ){
        ;  //TODO //maybe de-register the app
    }

    return ret;
}

static inline int inner_handle_heartbeat_from_app(int client_fd)
{
    int ret;
    hearbeat_to_proxy_t heartbeat_packet;
    ret = read_from_unix_socket_fd( client_fd, &heartbeat_packet, sizeof(heartbeat_packet));
    if (ret != 0) {
        return -1;
    }
    update_external_app_heartbeat_by_appID(heartbeat_packet.appID);
    return 0;
}

int handle_new_client_fd_accepted(int client_fd)
{
    /* WARNNING!!
     * make sure the packet you send-to/recv-from proxy client 
     * "MATCH" the send-to/recv-from action 
     *  in remote_app_registration() in external_app_proxy_client.c
     */

    int ret;
    bool is_notify_fd = 0;
    packet_to_proxy_header_t header;
    ack_from_proxy_header_t ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    ret = recv_from_unix_socket_fd( client_fd, &header, sizeof(header));
    if( (ret = recv_from_unix_socket_fd( client_fd, &header, sizeof(header)) ) != 0){
        fprintf( stderr, "%d: recv_from_unix_socket_fd ret: %d\n",__LINE__ , ret);
        return ret;
    }

    /* handle header error-check */
    if( header.packet_type != EA_PACKET_TYPE_REGISTER
        && header.packet_type != EA_PACKET_TYPE_NTF_UPDATE)
    {
        ack_packet.ret_val = EA_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER;
        send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return 0;
    }
    else if( header.api_id != API_ID_OF(remote_app_registration)){
        ack_packet.ret_val = EA_ERR_BAD_API_ID_BEFORE_REGISTER;
        send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return 0;
    }

    /* then read payload */

    if( header.packet_type == EA_PACKET_TYPE_REGISTER ){
        /* new registration or re-registration */
        
        app_registration_payload_t payload;

        if( (ret = recv_from_unix_socket_fd( client_fd, &payload, sizeof(payload)) ) != 0){
            fprintf( stderr, "%d: recv_from_unix_socket_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }
        app_obj_t* handling_app_p;
        handling_app_p = find_duplicate_app_obj_by_appID(payload.id);
        if( !handling_app_p ){  /* brand-new app registration */
            Malloc(handling_app_p, sizeof(app_obj_t), "handling_app_p");
            Malloc(handling_app_p->ea_info_p, sizeof(ea_info_t), "handling_app_p->ea_info_p");
            inner_handle_app_register(handling_app_p, &payload);
        }
        reset_external_app_both_fd(handling_app_p, client_fd);  /*this will set notify_fd 0*/
        handling_app_p->ea_info_p->pid = payload.pid;

        ack_from_proxy_header_t ack;
        ack.packet_type = EA_PACKET_TYPE_ACK;
        ack.ret_val = EA_ERR_OK;
        if( (ret = send_unix_socket_fd( client_fd, &ack, sizeof(ack)) ) != 0){
            fprintf( stderr, "%d: send_unix_socket_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }
    }
    else{  /* header.packet_type == EA_PACKET_TYPE_NTF_UPDATE */
        /* update notify channel after registration packet handled */
        notify_update_payload_t ntf_payload;
        ack_from_proxy_header_t ntf_ack;
        ntf_ack.packet_type = EA_PACKET_TYPE_ACK;

        if( (ret = recv_from_unix_socket_fd( client_fd, &ntf_payload, sizeof(ntf_payload)) ) != 0){
            fprintf( stderr, "%d: recv_from_unix_socket_fd ret: %d\n",__LINE__ , ret);
            return ret;
        }
        app_obj_t* handling_app_p;
        handling_app_p = find_duplicate_app_obj_by_appID(ntf_payload.id);
        if( !handling_app_p ){
            /* NOTIFY PACKET should not come from a new app */
            ntf_ack.ret_val = EA_ERR_LIB_REGISTRATION_INTERNAL_ERR;
        }
        else if( handling_app_p->ea_info_p->pid != ntf_payload.pid ) {
            /* NOTIFY PACKET should not come from another process 
             * different from one who link interact channel 
             */
            ntf_ack.ret_val = EA_ERR_LIB_REGISTRATION_INTERNAL_ERR;
        }
        else{
            ntf_ack.ret_val = EA_ERR_OK;
        }

        if( (ret = send_unix_socket_fd( client_fd, &ntf_ack, sizeof(ntf_ack)) ) != 0){
            fprintf( stderr, "%d: send_unix_socket_fd ret: %d\n", __LINE__ , ret);
            return ret;
        }

        if(ntf_ack.ret_val == EA_ERR_OK){
            notify_update_complete_payload_t ntf_ack_payload;
            ntf_ack_payload.RSU_id = config.RSU_id;
            ntf_ack_payload.RSU_lat = config.RSU_lat;
            ntf_ack_payload.RSU_lon = config.RSU_lon;
            ntf_ack_payload.RSU_lat = config.RSU_lat;
            ntf_ack_payload.RSU_region = config.RSU_region;
            strncpy(ntf_ack_payload.RSU_name, config.RSU_name, RSU_NAME_MAX_LEN);
            if( (ret = send_unix_socket_fd( client_fd, &ntf_ack, sizeof(ntf_ack)) ) != 0){
                fprintf( stderr, "%d: send_unix_socket_fd ret: %d\n", __LINE__ , ret);
                return ret;
            }
        }
    }

    return ret;
}

static inline int inner_handle_app_register( app_obj_t* app_p, void *payload_p)
{   
    app_registration_payload_t *pl_p = (app_registration_payload_t*)(payload_p);
    strncpy( app_p->name, pl_p->name, APP_NAME_MAX_LEN);
    app_p->dontSend2TC = pl_p->dontSend2TC;
    app_p->id = pl_p->id;
    app_p->priority = pl_p->priority;
    inner_set_external_app_callback_by_mask(app_p, pl_p->callback_register_mask);
    return app_register(app_p);  /* using existed function in application_registration.c */
}

//TODO
int check_all_external_app_heartbeat()
{
    ;
}



int32_t recv_from_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size)
{    
    int ret = recv(socket_fd, packet_p, packet_size, 0);

    if( ret == 0){  /* meaning that remote client might close the fd */
        return EA_ERR_SOCKET_DISCONNECT;
    }
    else if (ret  < 0){
        char* errno_str = strerror(errno);
        if( !errno_str ) 
            errno_str = "undefined/zero errno";
        fprintf(stderr, "recv_from_unix_socket_fd: recv() ret -1, errno is %s\n", errno_str);
        return EA_ERR_SOCKET_RECV;
    }
    return EA_ERR_OK;
}

int32_t send_to_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    if( (ret = send(socket_fd, packet_p, packet_size, MSG_NOSIGNAL)) == -1){
        current_errno = errno;
        char* errno_str = strerror(current_errno);
        if( !errno_str ) 
            errno_str = "undefined/zero errno";
        fprintf(stderr, "send_to_unix_socket_fd: send() ret -1, errno is %s\n", errno_str);
        if( errno == -EPIPE){
            return EA_ERR_SOCKET_DISCONNECT;
        }
        return EA_ERR_SOCKET_SEND;
    }
    return ret;
}


