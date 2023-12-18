#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h> 

#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_socket.h"
#include "log.h"

/* DANGER: this file: external_app_proxy_socket.c
 * will exist both is server-side and client-side
 * Except for the #include above, the two files should be "THE SAME" !! 
 * */
/* Most of the functions or variables in this file are used for client */

/* WARN: make sure the path MY_UNIX_SOCKET_PATH correctly locate the file for connect() 
 * i.e. the file should be the same file that is used by server (middlware EAP) */
//#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
#define MY_UNIX_SOCKET_PATH "../RSU_Controller_master/config/my_unix_socket_file_for_eap"
#define EAP_CONNECT_TIMEOUT_MS 10000   //10s == 10000ms

/* References:
 * connect(): https://man7.org/linux/man-pages/man2/connect.2.html
 * recv(): https://man7.org/linux/man-pages/man2/recv.2.html 
 * send(): https://man7.org/linux/man-pages/man2/send.2.html 
 */

/* since application might implement multi-thread program, ,
 * we add a mutex_lock to serialize their usage of the same channel */ 
pthread_mutex_t mutex_notify_fd = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_interact_fd = PTHREAD_MUTEX_INITIALIZER;

/* notify_fd and interact_fd are both file-scope, do not expose them to header file */
/* APP use notify_fd to get msg which is sent from middleware
   APP use interact_fd to send request/heartbeat to middleware and get ack */
static int notify_fd;
static int interact_fd; 
static int current_errno;
static uint32_t registered_appID;

static inline __attribute__((always_inline)) 
int32_t modify_socket_fd_block_setting(int socket_fd, bool set_to_block)
{    
    int flags;
    if ((flags = fcntl(socket_fd, F_GETFL)) == -1) {
        log_file_write_with_errno(
            "modify_socket_fd_block_setting(%s): fcntl(F_GETFL) ret -1\n", 
            set_to_block?"set":"unset" );
        return -1;
    }

    if( set_to_block )
        flags &= ~O_NONBLOCK;   
    else
        flags |= O_NONBLOCK;    /*set non-block flag*/
    
    if (fcntl(socket_fd, F_SETFL, flags) == -1) {
        log_file_write_with_errno(
            "modify_socket_fd_block_setting(%s): fcntl(F_SETFL) ret -1\n", 
            set_to_block?"set":"unset" );
        return -1;
    }
    return 0;
}

void set_register_appID(uint32_t appID)
{
    registered_appID = appID;
}

uint32_t get_register_appID()
{
    return registered_appID;
}

int32_t add_notify_fd_to_epoll(int* ep_fd)
{
    /* add add notify_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    /* no need to reserve struct epoll_event after epoll_ctl() */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = notify_fd;

    if(epoll_ctl( *ep_fd, EPOLL_CTL_ADD, notify_fd, &ev)){
        log_file_write_with_errno("add_notify_fd_to_epoll: epoll_ctl");
        return -EAL_ERR_EPOLL_SYSCALL;
    }
    return EAL_ERR_OK;
}

void lock_interact_channel()
{
    pthread_mutex_lock(&mutex_interact_fd);
}

void unlock_interact_channel()
{
    pthread_mutex_unlock(&mutex_interact_fd);
}

void lock_notify_channel()
{
    pthread_mutex_lock(&mutex_notify_fd);
}

void unlock_notify_channel()
{
    pthread_mutex_unlock(&mutex_notify_fd);
}

bool is_proxy_connected()
{
    return notify_fd && interact_fd;
}

int32_t is_interact_fd_set()
{
    return interact_fd != 0;
}

int32_t interact_fd_disconnect_from_proxy()
{
    int ret;
    errno = 0;
    ret = close(interact_fd);
    current_errno = errno;
    if(ret == -1){
        log_file_write_with_errno("%s: close() ret -1\n", __func__);
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    interact_fd = 0;
    return EAL_ERR_OK;
}

int32_t interact_fd_connect_to_proxy()
{   
    int ret; 
    if( (interact_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        log_file_write_with_errno("interact_fd_connect_to_proxy: socket() ret -1\n");
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    
    if( (ret = modify_socket_fd_block_setting(interact_fd, false)) == -1 ){
        return -EAL_ERR_SOCKET_SYSCALL;
    }

    /* sun_path in a char array with size = 108, be care of your "sk_addr" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    
    /* start the whole connection process */
    if( connect(interact_fd, (const struct sockaddr *)(&sk_addr), sizeof(struct sockaddr_un)) == -1){
        if (errno != EINPROGRESS) {
            log_file_write_with_errno("interact_fd_connect_to_proxy: connect() ret -1\n");
            ret = -EAL_ERR_SOCKET_SYSCALL;
            goto err_handle;
        }
        log_file_write("interact_fd_connect_to_proxy: connect() 1st-try ret EINPROGRESS\n"
                       "switch to epoll-timeout connection\n");
        
        int epfd, nfd;
        struct epoll_event ev, ev_ret[10];
        epfd = epoll_create(1);
        if (epfd < 0) {
            log_file_write_with_errno("interact_fd_connect_to_proxy: epoll_create(1) ret -1\n");
            ret = -EAL_ERR_EPOLL_SYSCALL;
            goto err_handle;
        }
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = interact_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, interact_fd, &ev) != 0) {
            log_file_write_with_errno("interact_fd_connect_to_proxy: epoll_ctl(EPOLL_CTL_ADD) ret -1\n");
            ret = -EAL_ERR_EPOLL_SYSCALL;
            goto err_handle;
        }
        nfd = epoll_wait(epfd, ev_ret, 10, EAP_CONNECT_TIMEOUT_MS);
        epoll_ctl(epfd, EPOLL_CTL_DEL, interact_fd, NULL);
        close(epfd);
        
        if (nfd < 0 || !(ev.events & EPOLLOUT)) {
            log_file_write("interact_fd_connect_to_proxy: connect timeout after %u ms\n", EAP_CONNECT_TIMEOUT_MS );
            ret = -EAL_ERR_SOCKET_CONNECT_TIMEOUT;
            goto err_handle;
        }

        int result;
        socklen_t result_len = sizeof(result);
        if (getsockopt(interact_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
            log_file_write_with_errno("interact_fd_connect_to_proxy: getsockopt ret < 0\n");
            ret = -EAL_ERR_SOCKET_SYSCALL;
            goto err_handle;
        }
        if (result != 0) {
            log_file_write("interact_fd_connect_to_proxy connect to server fail\n");
            ret = -EAL_ERR_SOCKET_CONNECT_FAIL;
            goto err_handle;
        }    
    }

    /* back to blocking mode */
    if( (ret = modify_socket_fd_block_setting(interact_fd, true)) == -1 ){
        ret = -EAL_ERR_SOCKET_SYSCALL;
        goto err_handle;
    }

    log_file_write("interact_fd_connect_to_proxy: connect success\n");
    return EAL_ERR_OK;

err_handle:
    interact_fd_disconnect_from_proxy();
    return ret;
}

int32_t interact_fd_recv_from_proxy(void* packet_p, size_t packet_size)
{
    int ret;
    errno = 0;
    ret = recv(interact_fd, packet_p, packet_size, 0);
    current_errno = errno;
    log_file_write("%s: recv() ret = %d\n", __func__, ret);
    if(ret == 0){   /* meaning that remote proxy might close the fd */
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_DISCONNECT;
    }
    else if( ret  < 0 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t interact_fd_send_to_proxy(void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    ret = send(interact_fd, packet_p, packet_size, MSG_NOSIGNAL);
    current_errno = errno;
    log_file_write("%s: send() ret = %d\n", __func__, ret);
    if( ret == -1 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: send() ret %d\n", __func__, ret);
        #endif
        if( errno == -EPIPE){
            return -EAL_ERR_SOCKET_DISCONNECT;
        }
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t is_notify_fd_set()
{
    return notify_fd != 0; 
}

int32_t notify_fd_disconnect_from_proxy()
{
    int ret;
    errno = 0;
    ret = close(notify_fd);
    current_errno = errno;
    if(ret == -1){
        log_file_write_with_errno("%s: close() ret -1\n", __func__);
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    notify_fd = 0;
    return EAL_ERR_OK;
}

int32_t notify_fd_connect_to_proxy()
{    
    int ret; 
    if( (notify_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        log_file_write_with_errno("notify_fd_connect_to_proxy: socket() ret -1\n");
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    
    if( (ret = modify_socket_fd_block_setting(notify_fd, false)) == -1 ){
        return -EAL_ERR_SOCKET_SYSCALL;
    }

    /* sun_path in a char array with size = 108, be care of your "sk_addr" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    
    /* start the whole connection process */
    if( connect(notify_fd, (const struct sockaddr *)(&sk_addr), sizeof(struct sockaddr_un)) == -1){
        if (errno != EINPROGRESS) {
            log_file_write_with_errno("notify_fd_connect_to_proxy: connect() ret -1\n");
            ret = -EAL_ERR_SOCKET_SYSCALL;
            goto err_handle;
        }
        log_file_write("notify_fd_connect_to_proxy: connect() 1st-try ret EINPROGRESS\n"
                       "switch to epoll-timeout connection\n");
        
        int epfd, nfd;
        struct epoll_event ev, ev_ret[10];
        epfd = epoll_create(1);
        if (epfd < 0) {
            log_file_write_with_errno("notify_fd_connect_to_proxy: epoll_create(1) ret -1\n");
            ret = -EAL_ERR_EPOLL_SYSCALL;
            goto err_handle;
        }
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = notify_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &ev) != 0) {
            log_file_write_with_errno("notify_fd_connect_to_proxy: epoll_ctl(EPOLL_CTL_ADD) ret -1\n");
            ret = -EAL_ERR_EPOLL_SYSCALL;
            goto err_handle;
        }
        nfd = epoll_wait(epfd, ev_ret, 10, EAP_CONNECT_TIMEOUT_MS);
        epoll_ctl(epfd, EPOLL_CTL_DEL, notify_fd, NULL);
        close(epfd);
        
        if (nfd < 0 || !(ev.events & EPOLLOUT)) {
            log_file_write("notify_fd_connect_to_proxy: connect timeout after %u ms\n", EAP_CONNECT_TIMEOUT_MS );
            ret = -EAL_ERR_SOCKET_CONNECT_TIMEOUT;
            goto err_handle;
        }

        int result;
        socklen_t result_len = sizeof(result);
        if (getsockopt(notify_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
            log_file_write_with_errno("notify_fd_connect_to_proxy: getsockopt ret < 0\n");
            ret = -EAL_ERR_SOCKET_SYSCALL;
            goto err_handle;
        }
        if (result != 0) {
            log_file_write("notify_fd_connect_to_proxy connect to server fail\n");
            ret = -EAL_ERR_SOCKET_CONNECT_FAIL;
            goto err_handle;
        }    
    }

    /* back to blocking mode */
    if( (ret = modify_socket_fd_block_setting(notify_fd, true)) == -1 ){
        ret = -EAL_ERR_SOCKET_SYSCALL;
        goto err_handle;
    }

    log_file_write("notify_fd_connect_to_proxy: connect success\n");
    return EAL_ERR_OK;

err_handle:
    notify_fd_disconnect_from_proxy();
    return ret;
}

int32_t notify_fd_recv_from_proxy(void* packet_p, size_t packet_size)
{    
    int ret;
    errno = 0;
    ret = recv(notify_fd, packet_p, packet_size, 0);
    current_errno = errno;
    log_file_write("%s: recv() ret = %d\n", __func__, ret);
    if( ret == 0 ){   /* meaning that remote proxy might close the fd */
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_DISCONNECT;
    }
    else if( ret  < 0 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t notify_fd_send_to_proxy(void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    ret = send(notify_fd, packet_p, packet_size, MSG_NOSIGNAL);
    current_errno = errno;
    log_file_write("%s: send() ret = %d\n", __func__, ret);
    if( ret == -1 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: send() ret %d\n", __func__, ret);
        #endif
        if( errno == -EPIPE){
            return -EAL_ERR_SOCKET_DISCONNECT;
        }
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

/* WARN:recommend middleware-api implementation use functions below  */
/* WARN:function below did NOT take the lock inside, 
   please take the lock by yourself using lock_interact_channel() and lock_notify_channel */

int32_t simple_send_heartbeat_to_proxy()
{
    int ret;
    packet_header_to_proxy_t hearbeat;
    hearbeat.packet_type = EA_PACKET_TYPE_REPORT;
    hearbeat.appID = registered_appID;
    
    errno = 0;
    ret = send(interact_fd, &hearbeat, sizeof(hearbeat), MSG_NOSIGNAL);
    current_errno = errno;
    log_file_write("%s: send() ret = %d\n", __func__, ret);
    if( ret == -1 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: send() ret = %d\n", __func__, ret);
        #endif
        if( errno == -EPIPE){
            return -EAL_ERR_SOCKET_DISCONNECT;
        } 
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t simple_send_request_header_to_proxy(int api_id) 
{   
    int ret;
    packet_header_to_proxy_t header;
    header.packet_type = EA_PACKET_TYPE_REQ;
    header.appID = registered_appID;
    header.api_id = api_id;

    errno = 0;
    ret = send(interact_fd, &header, sizeof(header), MSG_NOSIGNAL);
    current_errno = errno;
    log_file_write("%s: api->%s, send() ret = %d\n", __func__, api_id_str_arr[api_id], ret);
    if( ret == -1 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: api->%s, send() ret = %d\n", 
                                        __func__, api_id_str_arr[api_id], ret);
        #endif
        if( errno == -EPIPE){
            return -EAL_ERR_SOCKET_DISCONNECT;
        } 
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

int32_t simple_recv_ack_header_from_proxy(packet_header_from_proxy_t *ack_p, int api_id)
{
    int ret;
    memset(ack_p, 0, sizeof(packet_header_from_proxy_t));
    errno = 0;
    ret = recv(interact_fd, ack_p, sizeof(packet_header_from_proxy_t), 0);
    current_errno = errno;

    log_file_write("%s: api->%s, recv() ret = %d\n", __func__, api_id_str_arr[api_id], ret);
    if( ret == 0 ){   /* meaning that remote client might close the fd */
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("interact_fd_recv_from_proxy: recv() ret 0 (probably disconnected)\n");
        #endif
        return -EAL_ERR_SOCKET_DISCONNECT;
    }
    else if( ret < 0 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("interact_fd_recv_from_proxy: recv() ret <0\n");
        #endif
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    else if( ack_p->packet_type != EA_PACKET_TYPE_ACK ){
        #if ENABLE_LOGGING_EALIB_DETECTED_ERR
        log_file_write("%s: ack packet not EA_PACKET_TYPE_ACK\n");
        #endif
        ret = -EAL_ERR_PACKET_TYPE_NOT_MATCH;
    }
    else{
        ret = EAL_ERR_OK;
    }
    return ret;
}

/* this function should behaves like interact_fd_send_to_proxy() */
int32_t simple_send_packet_to_proxy(void* packet_p, size_t packet_size) 
{   
    int ret = 0;
    errno = 0;
    ret = send(interact_fd, packet_p, packet_size, MSG_NOSIGNAL);
    current_errno = errno;

    log_file_write("%s: send() ret = %d\n", __func__, ret);
    if( ret == -1 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: send() ret = %d\n", __func__, ret);
        #endif
        if( errno == -EPIPE){
            return -EAL_ERR_SOCKET_DISCONNECT;
        } 
        return -EAL_ERR_SOCKET_SYSCALL;
    }
    return EAL_ERR_OK;
}

/* this function should behaves like interact_fd_recv_from_proxy() */
int32_t simple_recv_packet_from_proxy(void* packet_p, size_t packet_size)
{
    int ret;
    errno = 0;
    ret = recv(interact_fd, packet_p, packet_size, 0);
    current_errno = errno;

    log_file_write("%s: recv() ret = %d\n", __func__, ret);
    if( ret == 0 ){   /* meaning that remote proxy might close the fd */
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_DISCONNECT;
    }
    else if( ret < 0 ){
        #if ENABLE_LOGGING_EALIB_INNER_SOCKET_ERR
        log_file_write_with_errno("%s: recv() ret = %d\n", __func__, ret);
        #endif
        return -EAL_ERR_SOCKET_SYSCALL;
    }

    return EAL_ERR_OK;
}

char* api_id_str_arr[] = {
    [API_ID_OF(special_reserved_api_id)] = "special_reserved_api_id",
    [API_ID_OF(reserved_id_for_heartbeat_comm)] = "reserved_id_for_heartbeat_comm",

    /* ea_external_app_proxy.h */
    [API_ID_OF(app_remote_register)] = "app_remote_register",

    /* ea_application_registration.h */
    [API_ID_OF(event_callback_msg_id_insert)] = "event_callback_msg_id_insert",

    /* com_packet_processing.h */
    [API_ID_OF(cloud_packet_tx)] = "cloud_packet_tx",
    [API_ID_OF(OBU_j2735_tx)] = "OBU_j2735_tx",
    [API_ID_OF(OBU_packet_tx)] = "OBU_packet_tx",
    [API_ID_OF(remote_com_send)] = "remote_com_send",

    /* config.h */
    [API_ID_OF(get_config_RSU_id)] = "get_config_RSU_id",
    [API_ID_OF(get_config_RSU_lat)] = "get_config_RSU_lat",
    [API_ID_OF(get_config_RSU_lon)] = "get_config_RSU_lon",
    [API_ID_OF(get_config_RSU_name)] = "get_config_RSU_name",
    [API_ID_OF(get_config_RSU_region)] = "get_config_RSU_region",
    [API_ID_OF(get_config_RSU_elev)] = "get_config_RSU_elev",

    /* traffic_signal_command_buffer.h */
    [API_ID_OF(command_buf_insert_effect_time)] = "command_buf_insert_effect_time",
    [API_ID_OF(command_buf_insert_adjustment)] = "command_buf_insert_adjustment",

    /* vms.h */    
    [API_ID_OF(vms_request_start)] = "vms_request_start",
    [API_ID_OF(vms_request_end)] = "vms_request_end",
    [API_ID_OF(vms_sync_evsp_prog)] = "vms_sync_evsp_prog",
    [API_ID_OF(vms_sync_then_start)] = "vms_sync_then_start",

    /* traffic_signal_status_updating.h */
    [API_ID_OF(get_traffic_signal_status)] = "get_traffic_signal_status",
    [API_ID_OF(get_current_traffic_signal_status)] = "get_current_traffic_signal_status",
    [API_ID_OF(get_current_phase)] = "get_current_phase",
    [API_ID_OF(get_current_step)] = "get_current_step",
    [API_ID_OF(get_current_second)] = "get_current_second",
    [API_ID_OF(get_SubPhaseCount)] = "get_SubPhaseCount",
    [API_ID_OF(get_SignalCount)] = "get_SignalCount",
    [API_ID_OF(get_plan_id)] = "get_plan_id",
    [API_ID_OF(get_control_status)] = "get_control_status",
    [API_ID_OF(get_PhaseOrder)] = "get_PhaseOrder",
    [API_ID_OF(get_remaining_time)] = "get_remaining_time",
    [API_ID_OF(get_SignalStatus)] = "get_SignalStatus",
    [API_ID_OF(get_total_compensation_second)] = "get_total_compensation_second",
    [API_ID_OF(get_compensation_buffer)] = "get_compensation_buffer",
    [API_ID_OF(set_control_status)] = "set_control_status",
    [API_ID_OF(get_original_tc_health_status)] = "get_original_tc_health_status",
    [API_ID_OF(get_next_SubPhaseID)] = "get_next_SubPhaseID",
    [API_ID_OF(get_prev_SubPhaseID)] = "get_prev_SubPhaseID",
};

char* ack_ret_val_str_arr[] = {
    /* no err */
    [EAL_ERR_OK] = "EAL_ERR_OK",

    /* OS-syscall or OS-lib-call err*/
    [EAL_ERR_SOCKET_SYSCALL -EAL_ERR_RESERVE] = "EAL_ERR_SOCKET_SYSCALL",
    [EAL_ERR_SOCKET_CONNECT_TIMEOUT -EAL_ERR_RESERVE] = "EAL_ERR_SOCKET_CONNECT_TIMEOUT",
    [EAL_ERR_SOCKET_CONNECT_FAIL -EAL_ERR_RESERVE] = "EAL_ERR_SOCKET_CONNECT_FAIL",
    [EAL_ERR_SOCKET_DISCONNECT -EAL_ERR_RESERVE] = "EAL_ERR_SOCKET_DISCONNECT",
    [EAL_ERR_EPOLL_SYSCALL -EAL_ERR_RESERVE] = "EAL_ERR_EPOLL_SYSCALL",
    [EAL_ERR_MEMORY_LIB -EAL_ERR_RESERVE] = "EAL_ERR_MEMORY_LIB",
    [EAL_ERR_J2735_MSG_ENCODE -EAL_ERR_RESERVE] = "EAL_ERR_J2735_MSG_ENCODE",
    [EAL_ERR_J2735_MSG_DECODE -EAL_ERR_RESERVE] = "EAL_ERR_J2735_MSG_DECODE",

    /* our middleware library detected err */
    [EAL_ERR_PACKET_TYPE_NOT_MATCH -EAL_ERR_RESERVE] = "EAL_ERR_PACKET_TYPE_NOT_MATCH",
    [EAL_ERR_PACKET_BAD_CONTENT -EAL_ERR_RESERVE] = "EAL_ERR_PACKET_BAD_CONTENT",
    [EAL_ERR_HEARTBEAT_TIMER_FD_SETTING -EAL_ERR_RESERVE] = "EAL_ERR_HEARTBEAT_TIMER_FD_SETTING",
    [EAL_ERR_APP_NOT_REGISTER_YET -EAL_ERR_RESERVE] = "EAL_ERR_APP_NOT_REGISTER_YET",
    [EAL_ERR_CALLBACK_NOT_DEFINED_IN_SYSTEM -EAL_ERR_RESERVE] = "EAL_ERR_CALLBACK_NOT_DEFINED_IN_SYSTEM",
    [EAL_ERR_CALLBACK_NOT_REGISTER_TO_MW -EAL_ERR_RESERVE] = "EAL_ERR_CALLBACK_NOT_REGISTER_TO_MW",
    [EAL_ERR_CALLBACK_NOT_SUPPORT_IN_CURRENT_VERSION -EAL_ERR_RESERVE] = "EAL_ERR_CALLBACK_NOT_SUPPORT_IN_CURRENT_VERSION",
    [EAL_ERR_CALLBACK_NOT_DEFINED_BY_CUR_APP -EAL_ERR_RESERVE] = "EAL_ERR_CALLBACK_NOT_DEFINED_BY_CUR_APP",
    [EAL_ERR_BAD_PARAMETER -EAL_ERR_RESERVE] = "EAL_ERR_BAD_PARAMETER",
    [EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER -EAL_ERR_RESERVE] = "EAL_ERR_BAD_PACKET_TYPE_BEFORE_REGISTER",
    [EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE -EAL_ERR_RESERVE] = "EAL_ERR_BAD_PACKET_TYPE_TO_MIDDLEWARE",
    [EAL_ERR_BAD_API_ID_BEFORE_REGISTER -EAL_ERR_RESERVE] = "EAL_ERR_BAD_API_ID_BEFORE_REGISTER",
    [EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT -EAL_ERR_RESERVE] = "EAL_ERR_IN_MIDDLEWARE_REGISTER_REJECT",
    [EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE -EAL_ERR_RESERVE] = "EAL_ERR_LIB_SEND_WRONG_PACKET_TYPE",
    [EAL_ERR_BAD_PACKET_TYPE_RECEIVE_FROM_MIDDLEWARE -EAL_ERR_RESERVE] = "EAL_ERR_BAD_PACKET_TYPE_RECEIVE_FROM_MIDDLEWARE",

    /* error detected when calling the actual api in middleware */
    [EAL_ERR_IN_MIDDLEWARE_ERR_COM_IO -EAL_ERR_RESERVE] = "EAL_ERR_IN_MIDDLEWARE_ERR_COM_IO",
    [EAL_ERR_IN_MIDDLEWARE_API_INTERNAL -EAL_ERR_RESERVE] = "EAL_ERR_IN_MIDDLEWARE_API_INTERNAL",
    [EAL_ERR_IN_MIDDLEWARE_HEARTBEAT_UPDATE_FOR_APP -EAL_ERR_RESERVE] = "EAL_ERR_IN_MIDDLEWARE_HEARTBEAT_UPDATE_FOR_APP",
};

char* get_str_by_err_code(int err_code)
{
    if( err_code == EAL_ERR_OK)  
        return "EAL_ERR_OK";

    int reverse_code = -1*err_code;
    if ( reverse_code <= EAL_ERR_RESERVE || reverse_code >= BOT_OF_EA_ERR_DEF)  
        return "Not EAL defined error";
    else
        return ack_ret_val_str_arr[ reverse_code -EAL_ERR_RESERVE ];
}
