#ifndef EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H
#define EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H

#include "typedefine.h"
#include "msg_queue.h"
#include "external_app_proxy_socket.h"

void event_middleware_restart_handler();

#define EAP_CBMSG_FORWARD_FUNC_OF(event_name) event_name ## _cbmsg_forward_func

/* "eap" stands for "external application proxy" */
/* "cbmsg" stands for "callback message" */
typedef int (*eap_cbmsg_forward_fp)(void*);

extern eap_cbmsg_forward_fp cbmsg_forward_fp_arr[EVENT_TYPE_NUMBER];

extern app_obj_t* proxy_handling_app_p;

#if FORWARD_SAME_FORMAT_OBU_MSG_TO_EA
typedef struct _wrapper_arg_for_obu_packet_t{
    msg_obj_t *msg_p;
    V2R_app_section_t *app_section_p;
} wrapper_arg_for_obu_packet_t;

int EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_rx_SAME_FORMAT)(void *app_section);
int EAP_CBMSG_FORWARD_FUNC_OF(on_OBU_packet_tx_SAME_FORMAT)(void *app_section);
#endif

#endif  /* EXTERNAL_APP_PROXY_CALLBACK_MSG_FORWARD_H */