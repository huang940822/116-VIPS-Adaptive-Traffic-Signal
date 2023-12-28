#ifndef EXTERNAL_APP_PROXY_SERVER_H
#define EXTERNAL_APP_PROXY_SERVER_H

#include <stdio.h>
#include <stdint.h>

#define simple_fatal_action_logger(action_str, ret_val) \
    do{                                                 \
        fprintf(stderr,"%s: %s ret: %d\n",              \
                __func__, (action_str), (ret_val) );    \
        log_file_write(                                 \
            "%s: %s ret: %d\n",                         \
            __func__, (action_str), (ret_val) );        \
    }while(0)

uint8_t get_current_eap_heartbeat_rc();
void *external_app_proxy_main_handler();

#define ENABLE_PERIODIC_HEARTBEAT_MONITOR 1
#define HEARTBEAT_MONITOR_START_OFFSET_S   100    //start heartbeat-monotoring after enter mainloop 100-s
#define HEARTBEAT_MONITOR_PERIOD_S     100        //after that, heartbeat-monotoring per 100-s
//app should send heartbeat to middleware at least inside 3 "period"
#define HEARTBEAT_MONITOR_ALLOWED_THERSHHOLD  3  
#define FORWARD_SAME_FORMAT_OBU_MSG_TO_EA 0

// logging options
#define ENABLE_LOGGING_EAP_DETECTED_ERR 1
#define ENABLE_PRINTING_EAP_DETECTED_ERR 0

#define ENABLE_LOGGING_EAP_MSG_FORWARDING 1
#define ENABLE_PRINTING_EAP_MSG_FORWARDING 0

#define ENABLE_LOGGING_EAP_REQUEST_HANDLING 1
#define ENABLE_PRINTING_EAP_REQUEST_HANDLING 0

#define ENABLE_LOGGING_EAP_HEARTBEAT_RECEIVING 1
#define ENABLE_PRINTING_EAP_HEARTBEAT_RECEIVING 0

#define ENABLE_LOGGING_EAP_INNER_SOCKET_ERR 1
#define ENABLE_PRINTING_EAP_INNER_SOCKET_ERR 0

/* below is used by external_app_proxy_api_wrapper.c */
int32_t eap_recv_packet_from_unix_sk(int socket_fd, void* packet_p, size_t packet_size);
int32_t eap_send_packet_to_unix_sk(int socket_fd, void* packet_p, size_t packet_size);


#endif  /* EXTERNAL_APP_PROXY_SERVER_H */