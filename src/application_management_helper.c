#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "application_registration.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "application_management_helper.h"

#include "external_app_proxy_socket.h"
#include "external_app_proxy_typedefine.h"
#include "external_app_proxy_server.h"

LOG_USE_MODULE(MIDDLEWARE);

static int current_errno;

/* return NULL means not found */
app_obj_t* get_app_obj_by_appID(uint8_t appID)
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

/* currently only external apps utilize unix socket, return NULL means not found */
app_obj_t* get_app_obj_by_unix_socket_fd(int unix_sk_fd)
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
            if (current->ea_info_p == 0){
                ;// not an external app, do nothing
            }
            else if(current->ea_info_p->notify_fd == unix_sk_fd){
                ret_p = current;
                break;
            }
            else if(current->ea_info_p->interact_fd == unix_sk_fd ){
                ret_p = current;
                break;
            }
            current = current->next;
        }
    }
    pthread_mutex_unlock(&mutex_app_list);

    return ret_p;
}

/* used when an external app connects and registers for first channel*/
/* this will set I-channel to new_interact_fd and set N-channel to 0 */
int reset_external_app_two_new_sk_fds(app_obj_t *app, int new_interact_fd)
{
    if(!app)
        return -1;
    if(!new_interact_fd)
        return -1;

    int ret;
    /* since now dispatcher and ea_app_proxy,
     * both might read/write app_list, we add a mutex_lock */
    pthread_mutex_lock(&mutex_app_list);
    if( app->ea_info_p ){
        app->ea_info_p->notify_fd = 0;
        app->ea_info_p->interact_fd = new_interact_fd;
        ret = 0;
    }
    else{
        ret = -1;
    }
    pthread_mutex_unlock(&mutex_app_list);
    return ret;
}

/* each external app has 2 channel,
 * this one used for updating second channel in the app_obj_t */
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

/* used when proxy detect an external app lose heartbeat for a long time */
/* or when send/recv to that APP's unix socket result in EAL_ERR_SOCKET_DISCONNECT */
/* WARNNING!! this function ASSUME the "CALLER" will TAKE "mutex_app_list" before call it */
int inner_close_both_channels_of_an_external_app(app_obj_t *app)
{
    int ret = 0;
    if(!app){
        ret = -1;
        goto end;
    }

    if( app->ea_info_p ){

        if( app->ea_info_p->interact_fd ){
            errno = 0;
            ret = close(app->ea_info_p->interact_fd);
            current_errno = errno;
            char* errno_str = strerror(errno);
            if( !errno_str ) errno_str = "undefined/zero errno";

            if(ret == -1){
                LOG_MSG_INFO(
                    "%s: close() app:%d socket_fd ret -1, strerror() shows: %s\n",
                    __func__, app->id, errno_str);

                goto end;
            }
            app->ea_info_p->interact_fd = 0;
        }

        if( app->ea_info_p->notify_fd ){
            errno = 0;
            ret = close(app->ea_info_p->notify_fd);
            current_errno = errno;
            char* errno_str = strerror(errno);
            if( !errno_str ) errno_str = "undefined/zero errno";

            if(ret == -1){
                LOG_MSG_INFO(
                    "%s: close() app:%d socket_fd ret -1, strerror() shows: %s\n",
                    __func__, app->id, errno_str);

                goto end;
            }
            app->ea_info_p->notify_fd = 0;
        }
    }
    else{
        LOG_MSG_INFO("err %s: invoked for internal app", __func__);
        /* this function should not be invoked for internal app */
        ret = -2;
    }

end:
    return ret;
}

/* used when send/recv to that APP's unix socket result in EAL_ERR_SOCKET_DISCONNECT */
/* this function will take mutex_lock */
int directly_close_both_channels_of_an_external_app(app_obj_t *app)
{
    int ret = 0;
    pthread_mutex_lock(&mutex_app_list);

    ret = inner_close_both_channels_of_an_external_app(app);

    pthread_mutex_unlock(&mutex_app_list);
    if(!ret){
        LOG_MSG_INFO( "%s: close app: %s's sockets", __func__, app->name);
    }
    return ret;
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
    if(pid==0)
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

