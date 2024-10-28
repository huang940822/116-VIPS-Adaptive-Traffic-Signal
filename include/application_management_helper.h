#ifndef APPLICATION_MANAGEMENT_HELPER_H
#define APPLICATION_MANAGEMENT_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <typedefine.h>

/* return NULL means not found */
app_obj_t* get_app_obj_by_appID(uint8_t appID);
app_obj_t* get_app_obj_by_unix_socket_fd(int unix_sk_fd);

int reset_external_app_two_new_sk_fds(app_obj_t *app, int socket_fd);
int update_external_app_notify_fd(app_obj_t *app, int socket_fd);

/* used when proxy detect an external app lose heartbeat for a long time */
/* or when send/recv to that APP's unix socket result in EAL_ERR_SOCKET_DISCONNECT */
/* WARNNING!! this function ASSUME the "CALLER" will TAKE "mutex_app_list" before call it */
int inner_close_both_channels_of_an_external_app(app_obj_t *app);

/* used when send/recv to that APP's unix socket result in EAL_ERR_SOCKET_DISCONNECT */
/* this function will TAKE "mutex_app_list" inside */
int directly_close_both_channels_of_an_external_app(app_obj_t *app);

int update_external_app_heartbeat_by_appID(uint8_t appID);
int update_external_app_pid(app_obj_t *app, int pid);

#endif  /* APPLICATION_MANAGEMENT_HELPER_H */