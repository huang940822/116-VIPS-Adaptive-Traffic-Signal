#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP_config.h"
#include "EVSP_timer_event.h"
#include "EVSP_touching_area.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"

EVSP_host_OBU_obj_t *EVSP_host_OBU_list_head = NULL;
pthread_mutex_t EVSP_host_OBU_list_mutex = PTHREAD_MUTEX_INITIALIZER;

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_new(char *OBU_id,
                                           uint8_t target_phase,
                                           EVSP_touching_area_t *area_ptr)
{
    EVSP_host_OBU_obj_t *host_OBU =
        (EVSP_host_OBU_obj_t *) malloc(sizeof(EVSP_host_OBU_obj_t));
    if (host_OBU == NULL) {
        set_memory_error();
        log_file_write_fatal_error("EVSP_host_OBU_obj_new: malloc");
        perror("EVSP_host_OBU_obj_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(host_OBU, 0, sizeof(EVSP_host_OBU_obj_t));
    }
    memcpy(host_OBU->OBU_id, OBU_id, OBU_ID_MAX_LEN);
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

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_insert(char *OBU_id,
                                              uint8_t target_phase,
                                              EVSP_touching_area_t *area_ptr)
{
    EVSP_host_OBU_obj_t *current;
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex); 
    /* empty list */
    if (EVSP_host_OBU_list_head == NULL) {
        EVSP_host_OBU_list_head = EVSP_host_OBU_obj_new(OBU_id, target_phase, area_ptr);
        current = EVSP_host_OBU_list_head;
    } else {
        current = EVSP_host_OBU_list_head;
        while (current->next) {
            if (strncmp(current->next->OBU_id, OBU_id, OBU_ID_MAX_LEN) == 0) {
                pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
                return NULL;
            }
            current = current->next;
        }
        current->next = EVSP_host_OBU_obj_new(OBU_id, target_phase, area_ptr);
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    return current;
}

EVSP_host_OBU_obj_t *EVSP_host_OBU_obj_search(char *OBU_id)
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);

    EVSP_host_OBU_obj_t *current = EVSP_host_OBU_list_head;
    /* traverse host OBU list */
    while (current != NULL) {
        /* host OBU already exist */
        if (strncmp(current->OBU_id, OBU_id, OBU_ID_MAX_LEN) == 0)
            break;
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    return current;
}

void EVSP_host_OBU_obj_delete(char *OBU_id)
{
    pthread_mutex_lock(&EVSP_host_OBU_list_mutex);
    EVSP_host_OBU_obj_t *previous = NULL, *current = EVSP_host_OBU_list_head;

    /* traverse host OBU list */
    while (current != NULL && strncmp(current->OBU_id, OBU_id, OBU_ID_MAX_LEN) != 0) {
        previous = NULL;
        current = current->next;
    }
    if (current == EVSP_host_OBU_list_head) {
        EVSP_host_OBU_list_head = current->next;
    } else if (current != NULL) {
        previous->next = current->next;
    }
    if (current != NULL) {
        delete_timer(current->host_OBU_packet_timer);
        delete_timer(current->host_OBU_list_timer);
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
                 current->OBU_id, current->target_phase);
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
    /* empty list */
    if (current == NULL) {
        pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
        return true;
    }

    /* traverse host OBU list */
    while (current != NULL) {
        if (current->target_phase == target_phase) {
            pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
            return false;
        }

        /* last node */
        if (current->next == NULL) {
            pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
            return true;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&EVSP_host_OBU_list_mutex);
    log_file_write_fatal_error("error checking EVSP host OBU resume");
    return true;
}