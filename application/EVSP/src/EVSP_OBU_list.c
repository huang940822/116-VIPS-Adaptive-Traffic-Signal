#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP_OBU_list.h"
#include "EVSP_config.h"
#include "EVSP_timer_event.h"
#include "EVSP_touching_area.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"

EVSP_host_OBU_obj_t *EVSP_host_OBU_list_head = NULL;
pthread_mutex_t EVSP_host_OBU_list_mutex = PTHREAD_MUTEX_INITIALIZER;

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_new(char *OBU_name,
                                           uint8_t target_phase,
                                           EVSP_touching_area_t *area_ptr)
{
    EVSP_host_OBU_obj_t *host_OBU;
    Malloc(host_OBU, sizeof(EVSP_host_OBU_obj_t), "EVSP_host_OBU_obj_new");

    memcpy(host_OBU->OBU_name, OBU_name, OBU_NAME_MAX_LEN);
    host_OBU->target_phase = target_phase;
    host_OBU->area_ptr = area_ptr;
    create_timer(&host_OBU->host_OBU_packet_timer, host_OBU,
                 EVSP_host_OBU_packet_timeout_timer_handler);
    set_timer(host_OBU->host_OBU_packet_timer, 0, 0,
              EVSP_config.evsp_host_obu_packet_timeout, 0);
    create_timer(&host_OBU->host_OBU_list_timer, host_OBU,
                 EVSP_host_OBU_list_timeout_timer_handler);
    set_timer(host_OBU->host_OBU_list_timer, 0, 0,
              EVSP_config.evsp_host_obu_list_timeout, 0);
    return host_OBU;
}

static inline void EVSP_host_OBU_obj_update(EVSP_host_OBU_obj_t *host_OBU, EVSP_OBU_update_info_t *info)
{
    if (info == NULL)
        return;
    host_OBU->lat = info->lat;
    host_OBU->lon = info->lon;
    host_OBU->direction = info->direction;
    host_OBU->speed = info->speed;
}

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_insert(char *OBU_name,
                                              uint8_t target_phase,
                                              EVSP_touching_area_t *area_ptr,
                                              EVSP_OBU_update_info_t *info)
{
    EVSP_host_OBU_obj_t *current, *previous;
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    /* empty list */
    if (EVSP_host_OBU_list_head == NULL) {
        EVSP_host_OBU_list_head = EVSP_host_OBU_obj_new(OBU_name, target_phase, area_ptr);
        current = EVSP_host_OBU_list_head;
    } else {
        current = EVSP_host_OBU_list_head;
        previous = current;
        while (current) {
            if (strncmp(current->OBU_name, OBU_name, OBU_NAME_MAX_LEN) == 0) {
                EVSP_host_OBU_obj_update(current, info);
                pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
                return NULL;
            }
            previous = current;
            current = current->next;
        }
        previous->next = EVSP_host_OBU_obj_new(OBU_name, target_phase, area_ptr);
        current = previous->next;
    }
    EVSP_host_OBU_obj_update(current, info);
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    return current;
}

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_search(char *OBU_name, EVSP_OBU_update_info_t *info)
{
    time_t cur_time = time(NULL);
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);

    EVSP_host_OBU_obj_t *current = EVSP_host_OBU_list_head;
    /* traverse host OBU list */
    while (current != NULL) {
        /* host OBU already exist */
        if (strncmp(current->OBU_name, OBU_name, OBU_NAME_MAX_LEN) == 0) {
            EVSP_host_OBU_obj_update(current, info);
            break;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    return current;
}

void EVSP_OBU_obj_terminate(char *OBU_name)
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    EVSP_host_OBU_obj_t *current = EVSP_host_OBU_list_head;

    /* traverse host OBU list */
    while (current != NULL && strncmp(current->OBU_name, OBU_name, OBU_NAME_MAX_LEN) != 0) {
        current = current->next;
    }
    if (current != NULL) {
        delete_timer(current->host_OBU_packet_timer);
        delete_timer(current->host_OBU_list_timer);
        current->terminate_time = time(NULL);
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
}

// 清除之前留下的 OBU_obj
void EVSP_OBU_obj_clean()
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    time_t cur_time = time(NULL);
    EVSP_host_OBU_obj_t *previous = NULL, *current = EVSP_host_OBU_list_head;
    while (current != NULL) {
        if (cur_time - current->terminate_time > EVSP_config.cooling_time) {
            if (current == EVSP_host_OBU_list_head) {
                EVSP_host_OBU_list_head = current->next;
                free(current);
                current = EVSP_host_OBU_list_head;
            } else {
                previous->next = current->next;
                free(current);
                current = previous->next;
            }
        } else {
            previous = current;
            current = current->next;
        }
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
}

void EVSP_host_OBU_obj_delete(char *OBU_name)
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    EVSP_host_OBU_obj_t *previous = NULL, *current = EVSP_host_OBU_list_head;

    /* traverse host OBU list */
    while (current != NULL && strncmp(current->OBU_name, OBU_name, OBU_NAME_MAX_LEN) != 0) {
        previous = current;
        current = current->next;
    }
    if (current == EVSP_host_OBU_list_head) {
        EVSP_host_OBU_list_head = current->next;
    } else if (current != NULL) {
        previous->next = current->next;
    }
    if (current != NULL) {
        free(current);
    }

    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
}

void EVSP_host_OBU_obj_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "EVSP host OBU list:");

    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    EVSP_host_OBU_obj_t *current = EVSP_host_OBU_list_head;
    /* empty list */
    if (current == NULL) {
        pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nempty list");
        log_file_write(log_content);
        return;
    }

    /* traverse host OBU list */
    while (current != NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n%s (%d)",
                 current->OBU_name, current->target_phase);
        /* last node */
        if (current->next == NULL) {
            pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
            log_file_write(log_content);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
}

bool EVSP_host_OBU_obj_resume(uint8_t target_phase)
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    EVSP_host_OBU_obj_t *current = EVSP_host_OBU_list_head;
    /* traverse host OBU list */
    while (current != NULL) {
        if (current->target_phase == target_phase && current->terminate_time == 0) {
            pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
            return false;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    return true;
}