#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "application_registration.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "application_helper.h"

#include "external_app_proxy_inner.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"

int reset_external_app_both_fd(app_obj_t *app, int socket_fd)
{   
    if(!app)
        return -1;
    if(!socket_fd)
        return -1;

    int ret;
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    pthread_mutex_lock(&mutex_app_list); 
    if( app->ea_info_p ){
        app->ea_info_p->notify_fd = 0;
        app->ea_info_p->interact_fd = socket_fd;
        ret = 0;
    }
    else{
        ret = -1;
    }
    pthread_mutex_unlock(&mutex_app_list);

    return ret;
}

int update_external_app_notify_fd(app_obj_t *app, int socket_fd)
{   
    if(!app)
        return -1;
    if(!socket_fd)
        return -1;

    int ret;
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    pthread_mutex_lock(&mutex_app_list); 
    if( app->ea_info_p ){
        app->ea_info_p->notify_fd = socket_fd;
        ret = 0;
    }
    else{
        ret = -1;
    }
    pthread_mutex_unlock(&mutex_app_list);
    return ret;
}

app_obj_t* find_duplicate_app_obj_by_appID(uint8_t appID)
{    
    app_obj_t* ret_p = 0;   /* assume not found */
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    pthread_mutex_lock(&mutex_app_list); 

    app_obj_t *current = app_list.next;
    
    if (current == NULL) {      
        ret_p = 0;   /* empty list */
    }
    else{
        /* traverse to last node */
        while (current != NULL) {
            if (current->id == appID) {
                ret_p = current;
                break;
            }
            current = current->next;
        }
    }
    pthread_mutex_unlock(&mutex_app_list); 

    return ret_p;   
}

int update_external_app_heartbeat_by_appID(uint8_t appID)
{   
    int ret;
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */ 
    pthread_mutex_lock(&mutex_app_list);

    app_obj_t *current = app_list.next;
    if (current == NULL) {      
        ret = -1;   /* empty list, appID does not exist */
    }
    
    /* traverse to last node */
    ret = -1;  /* assume not found */
    while (current != NULL) {
        if (current->id == appID) {
            if(current->ea_info_p){
                current->ea_info_p->heartbeat_rc = get_current_eap_heartbeat_rc();
                ret = 0;
            }
            else{   /*current->ea_info_p == 0*/
                ret = -1;   /* not external app*/
            }
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&mutex_app_list);
    return ret;   
}

int update_external_app_heartbeat(app_obj_t *app)
{   
    if(!app)
        return -1;

    if( app->ea_info_p ){
        /* since now dispatcher and ea_app_proxy,
         * both might read/write app_list, we add a mutex_lock */ 
        pthread_mutex_lock(&mutex_app_list); 
        app->ea_info_p->heartbeat_rc = get_current_eap_heartbeat_rc();
        pthread_mutex_unlock(&mutex_app_list);
        return 0;
    }
    else{
        return -1;
    }
}

int update_external_app_pid(app_obj_t *app, int pid)
{   
    if(!app)
        return -1;
    if(!pid)
        return -1;

    if( app->ea_info_p ){
        /* since now dispatcher and ea_app_proxy,
         * both might read/write app_list, we add a mutex_lock */ 
        pthread_mutex_lock(&mutex_app_list); 
        app->ea_info_p->pid = pid;
        pthread_mutex_unlock(&mutex_app_list);
        return 0;
    }
    else{
        return -1;
    }
}