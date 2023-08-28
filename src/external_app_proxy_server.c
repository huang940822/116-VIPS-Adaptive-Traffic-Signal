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

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_api_wrapper.h"
#include "external_app_proxy_server.h"

#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
#define EPOLL_MAX_EVENTS 64

/* The backlog argument defines the maximum length to which the
 * queue of pending connections for sockfd may grow. 
 * Reference: https://man7.org/linux/man-pages/man2/listen.2.html */
#define MY_BACKLOG 20

static int unix_listen_fd;  //only one, used for accepting new client
static int ep_fd;           //fd for epoll
static uint8_t proxy_cur_heartbeat;

int external_app_proxy_notify_callback(void* info);
uint8_t get_current_eap_heartbeat_rc();
void *external_app_proxy_handler();
int32_t read_from_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);
int32_t send_to_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);
app_obj_t* find_duplicate_id_app_obj( uint8_t req_id );
int handle_new_client_fd_accepted(int client_fd);
int handle_remote_client_request(int client_fd);
int inner_handle_app_register( app_obj_t* app_p, void* payload_p, bool duplicate_flag);
int inner_handle_heartbeat_from_app(int client_fd);
int inner_handle_request_by_api_id(int client_fd, uint32_t api_id);
uint8_t get_current_eap_heartbeat_rc();
void increase_eap_heartbeat_rc();
/* NOTICE, if you add new callback, you NEED to update function below */
void set_external_app_callback_by_mask(app_obj_t* app_obj_p, uint32_t mask);
/* above are function declarations */

//TODO
int external_app_proxy_notify_callback( void* info )
{
    ;
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

/* the main external_app_proxy server thread */
void *external_app_proxy_handler()
{   
    /* create a unix domain socket with MY_UNIX_SOCKET_PATH 
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
        ret = handle_heartbeat_from_app(client_fd);
    }
    else{   /* i.e., header.packet_type == EA_PACKET_TYPE_REQ */
        ret = handle_request_by_api_id(client_fd, header.api_id);
    }
    return ret;
}

/* this function will use wrapper_fp_arr, which will send unix packet to external app */
static inline int inner_handle_request_by_api_id(int client_fd, uint32_t api_id)
{   
    if ( client_fd == 0 ){
        fprintf(stderr, "err: handle_request_by_api_id: client_fd == 0\n");
        return -1;
    }
    if ( api_id == API_ID_OF(special_reserved_id) ){
        fprintf(stderr, "err: handle_request_by_api_id: api_id == %d\n", API_ID_OF(special_reserved_id));
        return -1;
    }
    if ( api_id >= NUM_OF_API_ID_DEFININITION ){
        fprintf(stderr, "err: handle_request_by_api_id: api_id >= %d\n", NUM_OF_API_ID_DEFININITION);
        return -1;
    }
    if ( api_id == API_ID_OF(remote_app_registration) ){
        fprintf(stderr, "err: handle_request_by_api_id: api_id == %d\n", API_ID_OF(remote_app_registration));
        return -1;
    }
    if ( api_id == API_ID_OF(app_main_loop_start) ){
        fprintf(stderr, "err: handle_request_by_api_id: api_id == %d\n", API_ID_OF(app_main_loop_start));
        return -1;
    }

    /* call the related wrapper function by its api_id */
    int ret;
    ret = (*wrapper_fp_arr[api_id])(client_fd);
    // maybe log the ret value
    return ret;
}

static inline int inner_handle_heartbeat_from_app(int client_fd)
{
    int ret;
    packet_to_proxy_hearbeat_t heartbeat_packet;
    ret = read_from_unix_socket_fd( client_fd, &heartbeat_packet, sizeof(heartbeat_packet));
    if (ret != 0) {
        return -1;
    }
    update_external_app_heartbeat_by_appID(heartbeat_packet.appID);
    return 0;
}

int handle_new_client_fd_accepted(int client_fd)
{
    int ret;
    packet_to_proxy_header_t header;
    ack_from_proxy_header_t ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    ret = read_from_unix_socket_fd( client_fd, &header, sizeof(header));
    if(ret != 0){
        return -1;
    }

    /* header error-check */
    if( header.packet_type != EA_PACKET_TYPE_REGI
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

    /* read payload */
    struct REQ_PAYLOAD_TYPE(remote_app_registration) payload;
    app_obj_t* handling_app_p;
    ret = read_from_unix_socket_fd( client_fd, &payload, sizeof(payload)); 

    if( handling_app_p = find_duplicate_app_obj_by_appID(payload.id) ){
        if( handling_app_p->ea_info_p->notify_fd != 0 ){
            if( header.packet_type == EA_PACKET_TYPE_NTF_UPDATE ){
                fprintf( stdout, "EA_ERR_BAD_PACKET_TYPE_REGISTER_ORDER detected, ea-library might went wrong\n");
                ack_packet.ret_val = EA_ERR_BAD_PACKET_TYPE_REGISTER_ORDER;
                send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
                return 0;
            }
            else{
                fprintf( stdout, "%s using id:%d is already registered\n", payload.name, payload.id);
                reset_external_app_both_fd(handling_app_p, client_fd);  /*this will set notify_fd 0*/
                handle_app_register(handling_app_p, &payload, true);
            }
        }
        else{   /* notify_fd == 0 */
            fprintf( stdout, "%s using id:%d updating notify channel\n", payload.name, payload.id);
            update_external_app_notify_fd(handling_app_p, client_fd);
        }
    }
    else{
        handling_app_p = calloc( 1, sizeof(app_obj_t) );
        handling_app_p->ea_info_p = calloc( 1, sizeof(ea_info_t) );
        reset_external_app_both_fd(handling_app_p, client_fd);  /*this will set notify_fd 0*/
        handle_app_register(handling_app_p, &payload, false);
    }

    ack_packet.ret_val = EA_ERR_OK;
    send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
    return 0;
}

int inner_handle_app_register( app_obj_t* app_p, void *payload_p, bool duplicate_flag)
{   
    int ret = 0;
    struct REQ_PAYLOAD_TYPE(remote_app_registration) *pl_p 
        = (struct REQ_PAYLOAD_TYPE(remote_app_registration)*)(payload_p);
    if(!duplicate_flag){
        /* a brand new app register */
        strncpy( app_p->name, pl_p->name, APP_NAME_MAX_LEN);
        app_p->dontSend2TC = pl_p->dontSend2TC;
        app_p->id = pl_p->id;
        app_p->priority = pl_p->priority;
        set_external_app_callback_by_mask(app_p, pl_p->callback_register_mask);
        ret = app_register(app_p);  /* func*/
    }
    
    update_external_app_pid(app_p, pl_p->pid);
    return ret;
}

/* NOTICE, if you add new callback, you NEED to update this function */
void set_external_app_callback_by_mask(app_obj_t* app_obj_p, uint32_t mask)
{
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_OBU_packet_rx) ) ){
        app_obj_p->on_OBU_packet_rx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_OBU_packet_tx) ) ){
        app_obj_p->on_OBU_packet_tx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_RSU_packet_rx) ) ){
        app_obj_p->on_RSU_packet_rx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_RSU_packet_tx) ) ){
        app_obj_p->on_RSU_packet_tx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_cloud_packet_rx) ) ){
        app_obj_p->on_cloud_packet_rx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_cloud_packet_tx) ) ){
        app_obj_p->on_cloud_packet_tx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_traffic_signal_command_tx) ) ){
        app_obj_p->on_traffic_signal_command_tx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_camera_packet_rx) ) ){
        app_obj_p->on_camera_packet_rx = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_registration) ) ){
        app_obj_p->on_registration = external_app_proxy_notify_callback;
    }
    if( mask |= ( 0x1 << BIT_SHIFT_OF(on_middleware_restart) ) ){
        app_obj_p->on_middleware_restart = external_app_proxy_notify_callback;
    }
}

/* below are only used by middleware itself */
int32_t read_from_unix_socket_fd(int socket_fd, void* ret_packet_p, size_t req_packet_size)
{    
    int ret = 0;
    if( (ret = read(socket_fd, ret_packet_p, req_packet_size)) == -1){
        fprintf(stderr, "read_from_unix_socket_fd: with fd:%d, ret:%d\n", socket_fd, ret);
        return EA_ERR_SOCKET_READ;
    }
    return ret;
}

int32_t send_to_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size)
{
    int ret = 0;
    if( (ret = write(socket_fd, packet_p, packet_size)) == -1){
        fprintf(stderr, "send_to_unix_socket_fd: with fd:%d, ret:%d\n", socket_fd, ret);
        return EA_ERR_SOCKET_WRITE;
    }
    return ret;
}


