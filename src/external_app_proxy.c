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
#include "application_registration.h"
#include "external_app_proxy_inner.h"
#include "external_app_proxy.h"

#define MAX_CLIENTS 200
#define EPOLL_MAX_EVENTS 32

/* The backlog argument defines the maximum length to which the
 * queue of pending connections for sockfd may grow. 
 * Reference: https://man7.org/linux/man-pages/man2/listen.2.html */

#define MY_BACKLOG 20

static int unix_listen_fd;  //only one, used for accepting new client
static int ep_fd;           //fd for epoll

int is_app_id_already_registered( void* req_packet_p);
int handle_new_client_register(int client_fd);
int handle_remote_client_request(int client_fd);

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

                handle_new_client_register(client_fd);
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

int handle_new_client_register(int client_fd){
    int ret;
    struct REQ_PACKET_TYPE(remote_app_registration) req_packet;
    memset(&req_packet, 0, sizeof(req_packet));

    struct ACK_PACKET_TYPE(remote_app_registration) ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.packet_type = EA_PACKET_TYPE_ACK;

    ret = read_from_unix_socket_fd( client_fd, &req_packet, sizeof(req_packet));

    if( req_packet.packet_type != EA_PACKET_TYPE_REGI){
        ack_packet.ret_val = EA_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER;
        send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return 0;
    }
    else if( req_packet.api_id != API_ID_OF(remote_app_registration) ){
        ack_packet.ret_val = EA_ERR_BAD_API_ID_BEFORE_REGISTER;
        send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return 0;
    }
    else if( is_app_id_already_registered( &req_packet ) ){
        ack_packet.ret_val = EA_ERR_APP_ID_ALREADY_REGISTER;
        send_to_unix_socket_fd( client_fd, &ack_packet, sizeof(ack_packet));
        return 0;
    }




    {
    uint32_t packet_type;
    uint32_t api_id;
    struct {
        char name[APP_NAME_MAX_LEN];
        uint32_t callback_register_mask;
        uint8_t id;
        uint8_t priority;
        uint8_t is_notify_channel;  /* otherwise, it's interact-channel*/
    } payload;
    };
    struct ACK_PACKET_TYPE(remote_app_registration){
        uint32_t packet_type;
        int ret_val;
        struct {
            uint8_t current_dontSend2TC;
        } payload;
    };


    register_msg_t msg;
    memset(&msg, 0, sizeof(register_msg_t));
    if( read(client_fd, &msg, sizeof(register_msg_t)) == -1){
        LOG_ERR("read()", -1);
    }

    client_node *cl_ptr = calloc(1, sizeof(client_node));
    strncpy(cl_ptr->client_name, msg.client_name, 32-1);
    cl_ptr->client_pid = msg.client_pid;
    cl_ptr->request_op_mask = msg.request_op_mask;
    cl_ptr->request_times = 0;  //reset
    cl_ptr->next = NULL;
    
    printf("new client:\n");
    printf("client name: %s\n", msg.client_name);
    printf("client pid: %d\n", cl_ptr->client_pid);
    printf("client op_mask: %x\n", cl_ptr->request_op_mask);
    printf("\n");

    // client_node *cursor = client_queue_head;
    // while( cursor != NULL ){
    //     if(client_queue_head == NULL){
    //         cursor = cl_ptr;
    //         break;
    //     }
    //     else{
    //         cursor = cursor->next;
    //     }
    // }

    register_ack_t ack;
    memset(&ack, 0, sizeof(register_ack_t));
    ack.sv_ret = RPC_E_OK;
    if( write(client_fd, &ack, sizeof(register_ack_t)) == -1){
        LOG_ERR("write()", -1);
    }
    return 0;
}

int handle_remote_client_request(int client_fd){
    ;//todo
    return 0;
}

int is_app_id_already_registered( void* req_packet_p )
{
    struct REQ_PACKET_TYPE(remote_app_registration) *packet_p = 
     (struct REQ_PACKET_TYPE(remote_app_registration) *)req_packet_p;
    
    app_obj_t *current = app_list.next;
    uint8_t num = 0;

    /* empty list */
    if (current == NULL) {
        app_list.next = app;
        return num;
    }

    /* traverse to last node */
    while (current->next != NULL) {
}



