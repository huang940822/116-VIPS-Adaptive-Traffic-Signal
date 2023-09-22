#ifndef EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H
#define EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H

#include "typedefine.h"
#include "msg_queue.h"
#include "external_app_proxy_socket.h"

/* "eap" stands for "external application proxy" */
/* "cbmsg" stands for "callback message" */
typedef int (*eap_cbmsg_forward_fp)(void* app_section);

extern eap_cbmsg_forward_fp cbmsg_forward_fp_arr[EVENT_TYPE_NUMBER];

extern app_obj_t* proxy_handling_app_p;

typedef struct _wrapper_arg_for_obu_packet_t{
    msg_obj_t *msg_p;
    V2R_app_section_t *app_section_p;
    OBU_object_t *object_p;
} wrapper_arg_for_obu_packet_t;

#endif  /* EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H */