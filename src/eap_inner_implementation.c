#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ea_external_app_proxy.h"
#include "eap_inner_implementation.h"

#define ENABLE_PERROR

/* unix_socket_fd is file-scope, do not expose to header file */
/* unix_socket_fd is used to communicate with middleware */
static int unix_socket_fd;    

/* below are only used in library functions by external application 
   middleware will not user*/

int32_t connect_to_proxy(){
    
    if( (unix_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        #ifdef ENABLE_PERROR
            perror("connect_to_proxy(): socket() ret -1\n");
        #endif
        return EA_ERR_SOCKET_CREATE;
    }

    /* sun_path in a char array with size = 108, be care of your "sk_addr" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);

    if( connect(unix_socket_fd, (const struct sockaddr *)(&sk_addr), sizeof(struct sockaddr_un)) == -1){
        #ifdef ENABLE_PERROR
            perror("connect_to_proxy(): connect() ret -1\n");
        #endif
        return EA_ERR_SOCKET_CONNECT;
    }
}

int32_t disconnect_from_proxy(){
    int ret;
    if( (ret = close(unix_socket_fd)) == -1){
        #ifdef ENABLE_PERROR
            perror("disconnect_from_proxy(): close() ret -1\n");
        #endif
        return EA_ERR_SOCKET_CLOSE;
    }
    return ret;
}

int32_t send_to_proxy( void* packet_p, size_t packet_size){

    int ret = 0;
    if( (ret = write(unix_socket_fd, packet_p, packet_size)) == -1){
        #ifdef ENABLE_PERROR
            perror("send_to_proxy(): write() ret -1\n");
        #endif
        return EA_ERR_SOCKET_WRITE;
    }
    return ret;
}

int32_t read_from_proxy( void* ret_packet_p, size_t req_packet_size){
    
    int ret = 0;
    if( ( ret = read(unix_socket_fd, ret_packet_p, req_packet_size)) == -1){
        #ifdef ENABLE_PERROR
            perror("read_from_proxy(): write() ret -1\n");
        #endif
        return EA_ERR_SOCKET_READ;
    }
    return ret;
}