#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "typedefine.h"
#include "application_registration.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"

uint8_t app_num;
app_obj_t app_list;
event_callback_t callback_list[EVENT_TYPE_NUMBER];

/* since now dispatcher and ea_app_proxy,
 * both might read/write app_list, we add a mutex_lock */ 
pthread_mutex_t mutex_app_list = PTHREAD_MUTEX_INITIALIZER;

/* since now dispatcher, ea_app_proxy, command_buf_send(), 
 * all might read/write callback_list, we add a mutex_lock */
pthread_mutex_t mutex_callback_list = PTHREAD_MUTEX_INITIALIZER;

app_obj_t* get_app_obj_by_name(char* name_p);

/*****************************************************************************
** Function:    event_callback_new
** Description: Create a new event callback node.
** Parameter:   app: application for registration
**              callback: callback function
** Return:      event_callback: address of new event callback node
******************************************************************************/
event_callback_t *event_callback_new(char *name, int priority, event_callback_id_choice id_chioce, int id, int (*callback)(void *))
{
    event_callback_t *event_callback =
        (event_callback_t *) malloc(sizeof(event_callback_t));
        
    app_obj_t* app_obj_p;

    if (event_callback == NULL) {
        set_memory_error();
        log_file_write_fatal_error("event_callback_new: malloc");
        perror("event_callback_new: malloc");
        exit(errno);
    }

    app_obj_p = get_app_obj_by_name(name);
    if(!app_obj_p){
        log_file_write_fatal_error("event_callback_new: get_app_obj_by_name() find no matching app");
        perror("event_callback_new: get_app_obj_by_name() find no matching app");
        exit(errno);
    }

    clear_memory_error();
    strncpy(event_callback->name, name, APP_NAME_MAX_LEN);
    event_callback->event_callback_id.choice = id_chioce;
    event_callback->event_callback_id.u.app_id = id;
    event_callback->priority = priority;
    event_callback->callback = callback;
    event_callback->next = NULL;
    event_callback->app_obj_p = app_obj_p;

    return event_callback;
}
/*****************************************************************************
** Function:    event_callback_msg_id_insert
** Description: Create a new event callback node.
** Parameter:   event_type_t: event type for registration
**              name: app name
**              priority : app priority
**              msg_id : msg id
**              callback: callback function
** Return:      event_callback: address of new event callback node
******************************************************************************/
void event_callback_msg_id_insert(event_type_t event_type,
                           char *name, int priority, DSRCmsgID msg_id,
                           int (*callback)(void *))
{
    /* since now dispatcher, ea_app_proxy, command_buf_send() 
     * all might read/write callback_list we add a mutex_lock */
    pthread_mutex_lock(&mutex_callback_list); 

    event_callback_t *previous = &callback_list[event_type];
    event_callback_t *current = previous->next;
    event_callback_t *event_callback = NULL;

    /* traverse callback list */
    while (current != NULL) {
        /* priority higher than next node, insert callback here */
        if (current->priority > priority) {
            event_callback = event_callback_new(name, priority, event_callback_id_msg_id, msg_id, callback);
            event_callback->next = current;
            previous->next = event_callback;
            // printf("insert callback\n");
            return;
        }
        previous = current;
        current = current->next;
    }
    previous->next = event_callback_new(name, priority, event_callback_id_msg_id, msg_id, callback);

    pthread_mutex_unlock(&mutex_callback_list);
}
/*****************************************************************************
** Function:    event_callback_insert
** Description: Insert callback function in callback list.
** Parameter:   head: list head of callback list
**              app: application for registration
**              callback: callback function
** Return:      none
******************************************************************************/
void event_callback_insert(event_callback_t *head,
                           app_obj_t *app,
                           int (*callback)(void *))
{
    /* since now dispatcher, ea_app_proxy, command_buf_send(), 
     * all might read/write callback_list, we add a mutex_lock */
    pthread_mutex_lock(&mutex_callback_list); 

    event_callback_t *previous = head;
    event_callback_t *current = head->next;
    event_callback_t *event_callback = NULL;

    /* traverse callback list */
    while (current != NULL) {
        /* callback with same app id already exist */
        if (current->event_callback_id.choice == event_callback_id_app_id &&
             current->event_callback_id.u.app_id == app->id) {
            // printf("callback with same app_id exist\n");
            return;
        }
        /* priority higher than next node, insert callback here */
        if (current->priority > app->priority) {
            event_callback = event_callback_new(app->name, app->priority, event_callback_id_app_id, app->id, callback);
            event_callback->next = current;
            previous->next = event_callback;
            // printf("insert callback\n");
            return;
        }
        previous = current;
        current = current->next;
    }
    previous->next = event_callback_new(app->name, app->priority, event_callback_id_app_id, app->id, callback);

    pthread_mutex_unlock(&mutex_callback_list);
}

/*****************************************************************************
** Function:    app_obj_insert
** Description: Insert app obj in app list.
**              Check if app with same name or id already exist.
** Parameter:   app: application for registration
** Return:      num: num of app
**               <0: insert failed
******************************************************************************/
int app_obj_insert(app_obj_t *app)
{
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    int ret;
    pthread_mutex_lock(&mutex_app_list); 

    app_obj_t *current = app_list.next;
    uint8_t num = 0;

    /* empty list */
    if (current == NULL) {
        app_list.next = app;
        goto unlock_ret;
    }

    /* traverse to last node */
    while (current->next != NULL) {
        if (strncmp(current->name, app->name, APP_NAME_MAX_LEN) == 0) {
            ret = APP_REGISTER_DUPLICATE_APP_NAME;
            goto unlock_ret;
        }
        if (current->id == app->id) {
            ret = APP_REGISTER_DUPLICATE_APP_ID;
            goto unlock_ret;
        }
        num++;
        current = current->next;
    }

    /* last node */
    if (strncmp(current->name, app->name, APP_NAME_MAX_LEN) == 0) {
        ret = APP_REGISTER_DUPLICATE_APP_NAME;
        goto unlock_ret;
    }
    if (current->id == app->id) {
        ret = APP_REGISTER_DUPLICATE_APP_ID;
        goto unlock_ret;
    }

    num++;
    ret = num;
    
unlock_ret:
    pthread_mutex_unlock(&mutex_app_list); 
    return num;
}

/*****************************************************************************
** Function:    app_register
** Description: Application registration.
**              Check value of stucture field valid.
** Parameter:   app: application for registration
** Return:        0: registration successfully
**               <0: registration failed
******************************************************************************/
int app_register(app_obj_t *app)
{
    // check app name
    if (strlen(app->name) == 0) {
        return APP_REGISTER_INVALID_APP_NAME;
    }
    // check app id
    if (app->id <= 0) {
        return APP_REGISTER_INVALID_APP_ID;
    }
    // check app priority
    if (app->priority <= 0) {
        return APP_REGISTER_INVALID_APP_PRIORITY;
    }

    // insert app in app list
    int ret = app_obj_insert(app);
    if (ret < 0) {
        printf("error inserting app in list: %d\n", ret);
        return ret;
    } else {
        app_num = ret + 1;
        if (app->on_OBU_packet_rx) {
            event_callback_insert(&callback_list[EVENT_OBU_PACKET_RX], app,
                                  app->on_OBU_packet_rx);
        }
        if (app->on_OBU_packet_tx) {
            event_callback_insert(&callback_list[EVENT_OBU_PACKET_TX], app,
                                  app->on_OBU_packet_tx);
        }
        if (app->on_RSU_packet_rx) {
            event_callback_insert(&callback_list[EVENT_RSU_PACKET_RX], app,
                                  app->on_RSU_packet_rx);
        }
        if (app->on_RSU_packet_tx) {
            event_callback_insert(&callback_list[EVENT_RSU_PACKET_TX], app,
                                  app->on_RSU_packet_tx);
        }
        if (app->on_cloud_packet_rx) {
            event_callback_insert(&callback_list[EVENT_CLOUD_PACKET_RX], app,
                                  app->on_cloud_packet_rx);
        }
        if (app->on_cloud_packet_tx) {
            event_callback_insert(&callback_list[EVENT_CLOUD_PACKET_TX], app,
                                  app->on_cloud_packet_tx);
        }
        if (app->on_camera_packet_rx) {
            event_callback_insert(&callback_list[EVENT_CAMERA_PACKET_RX], app,
                                  app->on_camera_packet_rx);
        }
        if (app->on_traffic_signal_command_tx) {
            event_callback_insert(
                &callback_list[EVENT_TRAFFIC_SIGNAL_COMMAND_TX], app,
                app->on_traffic_signal_command_tx);
        }
        if (app->on_registration) {
            event_callback_insert(&callback_list[EVENT_REGISTRATION], app,
                                  app->on_registration);
            app->on_registration(NULL);
        }

        return APP_REGISTER_ACCEPT;
    }
}

/* this function assume the caller have grabbed the mutex_callback_list  */
app_obj_t* get_app_obj_by_name(char* name_p){
    // check app name
    if ( !name_p ){
        return NULL;
    }

    if ( strlen(name_p) == 0 ) {
        return NULL;
    }

    app_obj_t *current = app_list.next;
    if (current == NULL) {      
        return NULL;   /* empty list */
    }
    else{
        /* traverse to last node */
        while (current != NULL) {
            if ( strncmp(current->name, name_p, APP_NAME_MAX_LEN) == 0) {
                return current;
            }
            current = current->next;
        }
    }
    return NULL;   /* empty list */
}

void event_callback_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%-50s%s",
             "callback_list[EVENT_TYPE_NAME]:",
             "APP_NAME1(APP_PRI1)-> APP_NAME2(APP_PRI2)-> ...");

    /* since now dispatcher, ea_app_proxy, command_buf_send(), 
     * all might read/write callback_list, we add a mutex_lock */
    pthread_mutex_lock(&mutex_callback_list); 

    event_callback_t *current;
    for (int i = 0; i < EVENT_TYPE_NUMBER; i++) {
        current = &callback_list[i];
        switch (i) {
        case EVENT_OBU_PACKET_RX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_OBU_PACKET_RX]:");
            break;
        case EVENT_OBU_PACKET_TX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_OBU_PACKET_TX]:");
            break;
        case EVENT_RSU_PACKET_RX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_RSU_PACKET_RX]:");
            break;
        case EVENT_RSU_PACKET_TX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_RSU_PACKET_TX]:");
            break;
        case EVENT_CLOUD_PACKET_RX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_CLOUD_PACKET_RX]:");
            break;
        case EVENT_CLOUD_PACKET_TX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_CLOUD_PACKET_TX]:");
            break;
        case EVENT_CAMERA_PACKET_RX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_CAMERA_PACKET_RX]:");
            break;
        case EVENT_TRAFFIC_SIGNAL_COMMAND_TX:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_TRAFFIC_SIGNAL_COMMAND_TX]:");
            break;
        case EVENT_REGISTRATION:
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\n%-50s",
                     "callback_list[EVENT_REGISTRATION]:");
            break;
        default:
            break;
        }
        // snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
        // strlen(log_content), "%-50s", log_content);
        while (current->next != NULL) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%s",
                     current->next->name);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "(%d)",
                     current->next->priority);
            current = current->next;
            if (current->next != NULL) {
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content), "%s", "-> ");
            }
        }
    }

    pthread_mutex_unlock(&mutex_callback_list);

    log_file_write(log_content);
}

void app_list_print()
{   
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    pthread_mutex_lock(&mutex_app_list); 

    app_obj_t *current = app_list.next;

    if (current == NULL) {
        return;
    }

    while (current != NULL) {
        printf("%s\n", current->name);
        current = current->next;
    }

    pthread_mutex_unlock(&mutex_app_list); 

    return;
}