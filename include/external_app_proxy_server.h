#ifndef EXTERNAL_APP_PROXY_SERVER_H
#define EXTERNAL_APP_PROXY_SERVER_H

#include <stdio.h>
#include <stdint.h>

#define simple_fatal_act_logger(action_str, ret_val) \
    do{                                              \
        fprintf(stderr,"%s: %s ret: %d\n",           \
                __func__, (action_str), (ret_val) ); \
        log_file_write_fatal_error(                  \
            "%s: %s ret: %d\n",                      \
            __func__, (action_str), (ret_val) );     \
    }while(0)

uint8_t get_current_eap_heartbeat_rc();
void *external_app_proxy_handler();

#define ENABLE_EXTERNAL_APP_HEARTBEAT_PERIODIC_CHECK 1
#define HEARTBEAT_CHECK_START_OFFSET_S   10    //send first heartbeat after enter mainloop 10-s
#define HEARTBEAT_CHECK_PERIOD_S     10        //after that, send heartbeat per 10-s
//app should send heartbeat to middleware at least inside "3" heartbeat check period
#define HEARTBEAT_CHECK_ALLOWED_THERSHHOLD  3  

/* below is used by external_app_proxy_api_wrapper.c */
int32_t recv_from_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);
int32_t send_to_unix_socket_fd(int socket_fd, void* packet_p, size_t packet_size);


#endif  /* EXTERNAL_APP_PROXY_SERVER_H */