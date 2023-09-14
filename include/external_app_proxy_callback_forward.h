#ifndef EXTERNAL_APP_PROXY_CALLBACK_FORWARD_H
#define EXTERNAL_APP_PROXY_CALLBACK_FORWARD_H

#include "typedefine.h"
#include "msg_queue.h"
#include "external_app_proxy_socket.h"

typedef int (*eap_callback_forward_fp)(void* app_section);

extern eap_callback_forward_fp callback_forward_fp_arr[EVENT_TYPE_NUMBER];

extern app_obj_t* proxy_handling_app_p;

typedef struct _wrapper_arg_for_obu_packet_t{
    msg_obj_t *msg_p;
    OBU_object_t *object_p;
} wrapper_arg_for_obu_packet_t;

#endif  /* EXTERNAL_APP_PROXY_CALLBACK_FORWARD_H */