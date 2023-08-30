#ifndef EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H
#define EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H

#include "external_app_proxy_inner.h"

#define PRINT_MSG_FOR_DEBUG 1

typedef int (*eap_callback_wrapper_fp)(void* app_section);

extern eap_callback_wrapper_fp callback_wrapper_fp_arr[EVENT_TYPE_NUMBER];

extern app_obj_t* proxy_handling_app_p;

#endif  /* EXTERNAL_APP_PROXY_CALLBACK_WRAPPER_H */