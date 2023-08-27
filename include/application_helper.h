#ifndef APPLICATION_HELPER_H
#define APPLICATION_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <typedefine.h>

app_obj_t* find_duplicate_app_obj_by_appID(uint8_t appID);
int update_external_app_heartbeat_by_appID(uint8_t appID);
int reset_external_app_both_fd(app_obj_t *app, int socket_fd);
int update_external_app_notify_fd(app_obj_t *app, int socket_fd);
int update_external_app_pid(app_obj_t *app, int pid);

#endif  /* APPLICATION_HELPER_H */