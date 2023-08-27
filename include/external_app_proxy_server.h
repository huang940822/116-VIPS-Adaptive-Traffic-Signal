#ifndef EXTERNAL_APP_PROXY_SERVER_H
#define EXTERNAL_APP_PROXY_SERVER_H

#include <stdio.h>
#include <stdint.h>

int external_app_proxy_notify_callback( void* info);
uint8_t get_current_eap_heartbeat_rc();
void *external_app_proxy_handler();

#endif  /* EXTERNAL_APP_PROXY_SERVER_H */