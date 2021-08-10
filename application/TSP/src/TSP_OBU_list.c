#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "log.h"
#include "TSP.h"
#include "timer_event.h"
#include "error_status.h"
#include "TSP_OBU_list.h"
#include "TSP_timer_event.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "TSP_config.h"

TSP_host_OBU_obj_t TSP_host_OBU_list;
pthread_mutex_t TSP_host_OBU_list_mutex = PTHREAD_MUTEX_INITIALIZER;

TSP_host_OBU_obj_t *TSP_host_OBU_obj_new(char *OBU_id, uint8_t target_phase)
{
    TSP_host_OBU_obj_t *host_OBU = (TSP_host_OBU_obj_t *)malloc(sizeof(TSP_host_OBU_obj_t));
    if (host_OBU == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_host_OBU_obj_new: malloc");
        perror("TSP_host_OBU_obj_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(host_OBU, 0, sizeof(TSP_host_OBU_obj_t));
    }
    memcpy(host_OBU->OBU_id, OBU_id, OBU_ID_MAX_LEN);
    host_OBU->target_phase = target_phase;
    create_timer(&host_OBU->host_OBU_list_timer, host_OBU, TSP_host_OBU_list_timeout_timer_handler);
    set_timer(host_OBU->host_OBU_list_timer, 0, 0, TSP_config.tsp_host_obu_list_timeout, 0);
    return host_OBU;
}

TSP_host_OBU_obj_t *TSP_host_OBU_obj_insert(char *OBU_id, uint8_t target_phase)
{
    pthread_mutex_lock(&TSP_host_OBU_list_mutex);
    TSP_host_OBU_obj_t *current = TSP_host_OBU_list.next;
    /* empty list */
    if (current == NULL) {
        TSP_host_OBU_list.next = TSP_host_OBU_obj_new(OBU_id, target_phase);
        pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
        TSP_host_OBU_obj_first_insert(OBU_id, target_phase);
        return TSP_host_OBU_list.next;
    }

    /* traverse host OBU list */
    while (current != NULL) {
        /* host OBU already exist */
        if (strncmp(current->OBU_id, OBU_id, OBU_ID_MAX_LEN) == 0) {
            set_timer(current->host_OBU_list_timer, 0, 0, TSP_config.tsp_host_obu_list_timeout, 0);
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            return current;
        }
        /* last node */
        if (current->next == NULL) {
            current->next = TSP_host_OBU_obj_new(OBU_id, target_phase);
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            TSP_host_OBU_obj_first_insert(OBU_id, target_phase);
            return current->next;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
    return NULL;
}

TSP_host_OBU_obj_t *TSP_host_OBU_obj_search(char *OBU_id)
{
    pthread_mutex_lock(&TSP_host_OBU_list_mutex);
    TSP_host_OBU_obj_t *current = TSP_host_OBU_list.next;
    /* empty list */
    if (current == NULL) {
        pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
        return NULL;
    }

    /* traverse host OBU list */
    while (current != NULL) {
        /* host OBU already exist */
        if (strncmp(current->OBU_id, OBU_id, OBU_ID_MAX_LEN) == 0) {
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            return current;
        }
        /* last node */
        if (current->next == NULL) {
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            return NULL;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
    return NULL;
}

void TSP_host_OBU_obj_delete(char *OBU_id)
{
    pthread_mutex_lock(&TSP_host_OBU_list_mutex);
    TSP_host_OBU_obj_t *current = &TSP_host_OBU_list;
    TSP_host_OBU_obj_t *target = NULL;
    /* empty list */
    if (current->next == NULL) {
        pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
        return;
    }

    /* traverse host OBU list */
    while (current->next != NULL) {
        /* host OBU already exist */
        if (strncmp(current->next->OBU_id, OBU_id, OBU_ID_MAX_LEN) == 0) {
            target = current->next;
            current->next = target->next;
            delete_timer(target->host_OBU_list_timer);
            free(target);
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
}

void TSP_host_OBU_obj_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "TSP host OBU list:");

    pthread_mutex_lock(&TSP_host_OBU_list_mutex);
    TSP_host_OBU_obj_t *current = TSP_host_OBU_list.next;
    /* empty list */
    if (current == NULL) {
        pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nempty list");
        log_file_write(log_content);
        return;
    }

    /* traverse host OBU list */
    while (current != NULL) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\n%s (%d)", current->OBU_id, current->target_phase);
        /* last node */
        if (current->next == NULL) {
            pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
            log_file_write(log_content);
            return;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&TSP_host_OBU_list_mutex);
}

void TSP_host_OBU_obj_first_insert(char *OBU_id, uint8_t target_phase)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    if (signal_status.SubPhaseID == 0) {
        return;
    }

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = TSP.id;
    command.app_priority = TSP.priority;
    command.target_phase = target_phase;
    strncpy(command.host_OBU_id, OBU_id, OBU_ID_MAX_LEN);

    uint8_t current_phase = signal_status.SubPhaseID;
    uint8_t current_step = signal_status.StepID;
    int ret = 0;

    //注意這邊 都會調整一秒鐘
    /* target_phase == current_phase */
    if (target_phase == current_phase && current_step == 1) {
        command.cycle = 0;
        command.phase = current_phase;
        command.adjustment = 1;
        ret = command_buf_insert_adjustment(&command);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, adjustment: %d (%d)", 
            command.cycle, command.phase, command.adjustment, ret);
    }
    if (target_phase == current_phase && current_step != 1) {
        command.cycle = 1;
        command.phase = target_phase;
        command.adjustment = 1;
        ret = command_buf_insert_adjustment(&command);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, adjustment: %d (%d)", 
            command.cycle, command.phase, command.adjustment, ret);
    }

    /* target_phase > current_phase */
    if (target_phase > current_phase) {
        command.cycle = 0;
        command.phase = target_phase;
        command.adjustment = 1;
        ret = command_buf_insert_adjustment(&command);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, adjustment: %d (%d)", 
            command.cycle, command.phase, command.adjustment, ret);
    }

    /* target_phase < current_phase */
    if (target_phase < current_phase) {
        command.cycle = 1;
        command.phase = target_phase;
        command.adjustment = 1;
        ret = command_buf_insert_adjustment(&command);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ncycle: %d, phase: %d, adjustment: %d (%d)", 
            command.cycle, command.phase, command.adjustment, ret);
    }
}