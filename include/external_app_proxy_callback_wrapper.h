#ifndef EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H
#define EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H

#include "typedefine.h"
#include "msg_queue.h"
#include "external_app_proxy_inner.h"

#define PRINT_MSG_FOR_DEBUG 1

typedef int (*eap_callback_wrapper_fp)(void* app_section);

extern eap_callback_wrapper_fp callback_wrapper_fp_arr[EVENT_TYPE_NUMBER];

extern app_obj_t* proxy_handling_app_p;

typedef struct _wrapper_var_for_obu_packet_t{
    msg_obj_t *msg_p;
    OBU_object_t *object_p;
}wrapper_var_for_obu_packet_t;

#endif  /* EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H */