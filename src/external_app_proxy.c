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

int handle_new_client_register(int client_fd);
int handle_remote_client_request(int client_fd);

void* external_app_proxy_handler()
{   
    /* create a unix domain socket with MY_UNIX_SOCKET_PATH 

     * unlink, if socket already exists */
    struct stat statbuf;
    if( stat (MY_UNIX_SOCKET_PATH, &statbuf) == 0) {
        log_file_write("external_app_proxy_handler: MY_UNIX_SOCKET_PATH is already exist");
        if (unlink (MY_UNIX_SOCKET_PATH) == -1){
	        log_file_write_fatal_error("external_app_proxy_handler: unlink");
        }
    }

    /* step1: create a unix domain socket */
    if( (unix_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
	    log_file_write_fatal_error("external_app_proxy_handler: socket");
    }

    /* step2: bind the socket to MY_UNIX_SOCKET_PATH */
    /* sun_path in a char array with size = 108, be care of your "sk_addr.sun_path" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    if( bind(unix_listen_fd, (const struct sockaddr *) &sk_addr, sizeof (struct sockaddr_un)) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: bind");
    }

    /* step3: listen using unix_listen_fd */
    if( listen(unix_listen_fd, MY_BACKLOG) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: listen");
    }

    /* step4 create epoll fd */
    if( (ep_fd = epoll_create1(0)) == -1){
        log_file_write_fatal_error("external_app_proxy_handler: epoll_create1");
    }

    /* step5 add unix_listen_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = unix_listen_fd;
    if(epoll_ctl(ep_fd, EPOLL_CTL_ADD, unix_listen_fd, &ev)){
        log_file_write_fatal_error("external_app_proxy_handler: epoll_ctl");
    }

    struct epoll_event ev_arr[EPOLL_MAX_EVENTS];
    log_file_write("external_app_proxy_handler: is ready to enter main loop");
    int client_fd;       //temporary store client that we are communicating with.
    int num_ev_ready;    //number of events which are ready, returned by epoll_wait()

    /* start polling connection using epoll_wait */
    while(1){ 
        /* arg1: fd for epoll 
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
                }

                log_file_write("new client_fd to external_app_proxy: %d\n", client_fd);
                fprintf(stderr, "new client_fd to external_app_proxy: %d\n", client_fd);

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

    log_file_write_fatal_error("external_app_proxy_handler: thread exit");
}

int handle_new_client_register(int client_fd){
    ;//todo
    return 0;
}

int handle_remote_client_request(int client_fd){
    ;//todo
    return 0;
}




