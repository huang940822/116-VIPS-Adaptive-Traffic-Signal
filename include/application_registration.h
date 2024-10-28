#ifndef APPLICATION_REGISTRATION_H
#define APPLICATION_REGISTRATION_H

#include "typedefine.h"

extern uint8_t app_num;
extern app_obj_t app_list;
extern event_callback_t callback_list[EVENT_TYPE_NUMBER];

int app_register(app_obj_t *app);

/* since now dispatcher and ea_app_proxy,
 * both might read/write app_list, we add a mutex_lock */ 
extern pthread_mutex_t mutex_app_list;

/* since now dispatcher, ea_app_proxy, command_buf_send(), 
 * all might read/write callback_list, we add a mutex_lock */
extern pthread_mutex_t mutex_callback_list;

void app_list_print();
void event_callback_print();
void event_callback_msg_id_insert(event_type_t event_type,
                           char *name, int priority, DSRCmsgID msg_id,
                           int (*callback)(void *));
/* Return codes of application register */
typedef enum app_register_err {
    APP_REGISTER_ACCEPT = 0,
    APP_REGISTER_INVALID_APP_NAME = -1,
    APP_REGISTER_INVALID_APP_ID = -2,
    APP_REGISTER_INVALID_APP_PRIORITY = -3,
    APP_REGISTER_DUPLICATE_APP_NAME = -4,
    APP_REGISTER_DUPLICATE_APP_ID = -5
} app_register_err_t;

#endif  /* APPLICATION_REGISTRATION_H */