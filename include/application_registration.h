#include "typedefine.h"

extern uint8_t app_num;
extern app_obj_t app_list;
extern event_callback_t callback_list[EVENT_TYPE_NUMBER];

int app_register(app_obj_t *);
void app_list_print();
void event_callback_print();

/* Return codes of application register */
typedef enum app_register_err {
    APP_REGISTER_ACCEPT = 0,
    APP_REGISTER_INVALID_APP_NAME = -1,
    APP_REGISTER_INVALID_APP_ID = -2,
    APP_REGISTER_INVALID_APP_PRIORITY = -3,
    APP_REGISTER_DUPLICATE_APP_NAME = -4,
    APP_REGISTER_DUPLICATE_APP_ID = -5
} app_register_err_t;