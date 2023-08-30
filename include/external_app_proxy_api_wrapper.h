#ifndef EXTERNAL_APP_PROXY_API_WRAPPER_H
#define EXTERNAL_APP_PROXY_API_WRAPPER_H

#include "external_app_proxy_inner.h"

#define PRINT_MSG_FOR_DEBUG 1

typedef int (*eap_api_wrapper_fp)(int client_fd);

extern eap_api_wrapper_fp api_wrapper_fp_arr[NUM_OF_API_ID_DEFININITION];

#endif  /* EXTERNAL_APP_PROXY_API_WRAPPER_H */