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

#define MY_UNIX_SOCKET_PATH    "/tmp/comm_unix_sk.socket"
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
/* middleware use notify_fd to send msg to app 
   app use interact_fd to send request/heartbeat to middleware and get ack */
static int notify_fd;
static int interact_fd; 
static int current_errno;
static uint32_t register_app_id;

static inline __attribute__((always_inline)) 
int32_t modify_socket_fd_block_setting(int socket_fd, bool set_to_block)
{    
    int flags;
    if ((flags = fcntl(socket_fd, F_GETFL)) == -1) {
        log_file_write_fatal_with_errno(
            "modify_socket_fd_block_setting(%s): fcntl(F_GETFL) ret -1\n", 
            set_to_block?"set":"unset" );
        return -1;
    }

    if( set_to_block )
        flags &= ~O_NONBLOCK;   
    else
        flags |= O_NONBLOCK;    /*set non-block flag*/
    
    if (fcntl(socket_fd, F_SETFL, flags) == -1) {
        log_file_write_fatal_with_errno(
            "modify_socket_fd_block_setting(%s): fcntl(F_SETFL) ret -1\n", 
            set_to_block?"set":"unset" );
        return -1;
    }
    return 0;
}

/* below are only used in library functions used by external application,
   middleware itself will not use */

void set_register_app_id(uint32_t appID)
{
    register_app_id = appID;
}

uint32_t get_register_app_id()
{
    return register_app_id;
}

int32_t add_notify_fd_to_epoll(int* ep_fd)
{
    /* add add notify_fd to epoll using epoll_ctl(EPOLL_CTL_ADD) */
    /* no need to reserve struct epoll_event after epoll_ctl() */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = notify_fd;

    if(epoll_ctl( *ep_fd, EPOLL_CTL_ADD, notify_fd, &ev)){
        log_file_write_fatal_with_errno("add_notify_fd_to_epoll: epoll_ctl");
        return EA_ERR_NOTIFY_FD_ADD_TO_EPOLL;
    }
    return EA_ERR_OK;
}

int32_t is_interact_fd_linked()
{
    return interact_fd == 0;
}

int32_t interact_fd_disconnect_from_proxy()
{
    int ret;
    if( (ret = close(interact_fd)) == -1){
        log_file_write_fatal_with_errno(
            "interact_fd_disconnect_from_proxy: close() ret -1\n");
        return EA_ERR_SOCKET_CLOSE;
    }
    interact_fd = 0;
    return EA_ERR_OK;
}

int32_t interact_fd_connect_to_proxy()
{   
    int ret; 
    if( (interact_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        log_file_write_fatal_with_errno("interact_fd_connect_to_proxy: socket() ret -1\n");
        return EA_ERR_SOCKET_CREATE;
    }
    
    if( (ret = modify_socket_fd_block_setting(interact_fd, false)) == -1 ){
        return EA_ERR_SOCKET_BLOCK_SETTING;
    }

    /* sun_path in a char array with size = 108, be care of your "sk_addr" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    
    /* start the whole connection process */
    if( connect(interact_fd, (const struct sockaddr *)(&sk_addr), sizeof(struct sockaddr_un)) == -1){
        if (errno != EINPROGRESS) {
            log_file_write_fatal_with_errno("interact_fd_connect_to_proxy: connect() ret -1\n");
            goto err_handle;
        }
        log_file_write("interact_fd_connect_to_proxy: connect() 1st-try ret EINPROGRESS\n"
                       "switch to epoll-timeout connection\n");
        
        int epfd, nfd;
        struct epoll_event ev, ev_ret[10];
        epfd = epoll_create(1);
        if (epfd < 0) {
            log_file_write_fatal_with_errno("interact_fd_connect_to_proxy: epoll_create(1) ret -1\n");
            goto err_handle;
        }
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = interact_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, interact_fd, &ev) != 0) {
            log_file_write_fatal_with_errno("interact_fd_connect_to_proxy: epoll_ctl(EPOLL_CTL_ADD) ret -1\n");
            goto err_handle;
        }
        nfd = epoll_wait(epfd, ev_ret, 10, EAP_CONNECT_TIMEOUT_MS);
        epoll_ctl(epfd, EPOLL_CTL_DEL, interact_fd, NULL);
        close(epfd);
        
        if (nfd < 0 || !(ev.events & EPOLLOUT)) {
            log_file_write("interact_fd_connect_to_proxy: connect timeout\n");
            goto err_handle;
        }

        int result;
        socklen_t result_len = sizeof(result);
        if (getsockopt(interact_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
            log_file_write_fatal_with_errno("interact_fd_connect_to_proxy: getsockopt ret < 0\n");
            goto err_handle;
        }
        if (result != 0) {
            log_file_write("interact_fd_connect_to_proxy connect to server fail\n");
            goto err_handle;
        }    
    }

    /* back to blocking mode */
    if( (ret = modify_socket_fd_block_setting(interact_fd, true)) == -1 ){
        goto err_handle;
    }

    log_file_write("interact_fd_connect_to_proxy: connect success\n");
    return EA_ERR_OK;

err_handle:
    interact_fd_disconnect_from_proxy();
    return EA_ERR_SOCKET_CONNECT;
}

int32_t interact_fd_recv_from_proxy(void* packet_p, size_t packet_size)
{
    int ret = recv(interact_fd, packet_p, packet_size, 0);
    if(ret == 0){   /* meaning that remote client might close the fd */
        return EA_ERR_SOCKET_DISCONNECT;
    }
    else if( ret  < 0 ){
        log_file_write_fatal_with_errno("interact_fd_recv_from_proxy: recv() ret -1\n");
        return EA_ERR_SOCKET_RECV;
    }
    return ret;
}

int32_t interact_fd_send_to_proxy(void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    if( (ret = send(interact_fd, packet_p, packet_size, MSG_NOSIGNAL)) == -1){
        current_errno = errno;
        log_file_write_fatal_with_errno("interact_fd_send_to_proxy: send() ret -1\n");
        if( errno == -EPIPE){
            return EA_ERR_SOCKET_DISCONNECT;
        } 
        return EA_ERR_SOCKET_SEND;
    }
    return ret;
}

int32_t is_notify_fd_linked()
{
    return notify_fd == 0; 
}

int32_t notify_fd_disconnect_from_proxy()
{
    int ret;
    if( (ret = close(notify_fd)) == -1){
        log_file_write_fatal_with_errno("notify_fd_disconnect_from_proxy: close() ret -1\n");
        return EA_ERR_SOCKET_CLOSE;
    }
    notify_fd = 0;
    return ret;
}

int32_t notify_fd_connect_to_proxy()
{    
    int ret; 
    if( (notify_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: socket() ret -1\n");
        return EA_ERR_SOCKET_CREATE;
    }

    if( (ret = modify_socket_fd_block_setting(notify_fd, false)) == -1 ){
        return EA_ERR_SOCKET_BLOCK_SETTING;
    }

    /* sun_path in a char array with size = 108, be care of your "sk_addr" */
    struct sockaddr_un sk_addr;
    memset (&sk_addr, 0, sizeof (struct sockaddr_un));
    sk_addr.sun_family = AF_UNIX;
    strncpy (sk_addr.sun_path, MY_UNIX_SOCKET_PATH, sizeof(sk_addr.sun_path) - 1);
    
    /* start the whole connection process */
    if( connect(notify_fd, (const struct sockaddr *)(&sk_addr), sizeof(struct sockaddr_un)) == -1){
        if (errno != EINPROGRESS) {
            log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: connect() ret -1\n");
            goto err_handle;
        }
        log_file_write("notify_fd_connect_to_proxy: connect() 1st-try ret EINPROGRESS\n"
                       "switch to epoll-timeout connection\n");

        int epfd, nfd;
        struct epoll_event ev, ev_ret[10];
        epfd = epoll_create(1);
        if (epfd < 0) {
            log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: epoll_create(1) ret -1\n");
            goto err_handle;
        }
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = notify_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &ev) != 0) {
            log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: epoll_ctl(EPOLL_CTL_ADD) ret -1\n");
            goto err_handle;
        }
        nfd = epoll_wait(epfd, ev_ret, 10, EAP_CONNECT_TIMEOUT_MS);
        epoll_ctl(epfd, EPOLL_CTL_DEL, notify_fd, NULL);
        close(epfd);
        
        if (nfd < 0 || !(ev.events & EPOLLOUT)) {
            log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: timeout\n");
            goto err_handle;
        }

        int result;
        socklen_t result_len = sizeof(result);
        if (getsockopt(notify_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0) {
            log_file_write_fatal_with_errno("notify_fd_connect_to_proxy: getsockopt ret < 0\n");
            goto err_handle;
        }
        if (result != 0) {
            log_file_write("notify_fd_connect_to_proxy: connect to server fail\n");
            goto err_handle;
        }     
    }

    /* back to blocking mode */
    if( (ret = modify_socket_fd_block_setting(notify_fd, true)) == -1 ){
        goto err_handle;
    }

    log_file_write("notify_fd_connect_to_proxy: connect success\n");
    return EA_ERR_OK;

err_handle:
    notify_fd_disconnect_from_proxy();
    return EA_ERR_SOCKET_CONNECT;
}

int32_t notify_fd_recv_from_proxy(void* packet_p, size_t packet_size)
{    
    int ret = recv(notify_fd, packet_p, packet_size, 0);
    if(ret == 0){   /* meaning that remote client might close the fd */
        return EA_ERR_SOCKET_DISCONNECT;
    }
    else if( ret  < 0 ){
        log_file_write_fatal_with_errno("notify_fd_recv_from_proxy: recv() ret -1\n");
        return EA_ERR_SOCKET_RECV;
    }
    return ret;
}

int32_t notify_fd_send_to_proxy(void* packet_p, size_t packet_size)
{
    int ret = 0;
    errno = 0;
    if( (ret = send(notify_fd, packet_p, packet_size, MSG_NOSIGNAL)) == -1){
        current_errno = errno;
        log_file_write_fatal_with_errno("notify_fd_send_to_proxy: send() ret -1\n");
        if( errno == -EPIPE){
            return EA_ERR_SOCKET_DISCONNECT;
        }
        return EA_ERR_SOCKET_SEND;
    }
    return ret;
}

char* api_id_str_arr[] = {
    [API_ID_OF(special_reserved_api_id)] = "special_reserved_api_id",
    
    /* ea_external_app_proxy.h */
    [API_ID_OF(remote_app_registration)] = "remote_app_registration",
    [API_ID_OF(app_main_loop_start)] = "app_main_loop_start",

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



