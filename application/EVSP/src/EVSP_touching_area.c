#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP_touching_area.h"
#include "error_status.h"
#include "log.h"

EVSP_touching_area_plan_list_t *EVSP_touching_area_plan_list_head = NULL;

EVSP_touching_area_plan_list_t *EVSP_touching_area_plan_new(char *file_name,
                                                            uint8_t plan_id)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    EVSP_touching_area_plan_list_t *plan =
        (EVSP_touching_area_plan_list_t *) malloc(
            sizeof(EVSP_touching_area_plan_list_t));
    if (plan == NULL) {
        set_memory_error();
        log_file_write_fatal_error("EVSP_touching_area_plan_new: malloc");
        perror("EVSP_touching_area_plan_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(plan, 0, sizeof(EVSP_touching_area_plan_list_t));
    }
    plan->plan_id = plan_id;

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncpy(file_path, TOUCHING_AREA_DIR, sizeof(TOUCHING_AREA_DIR));
    strncat(file_path, file_name, sizeof(file_path));

    /* Open file */
    FILE *fp;
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", file_path);
    } else {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "%s opened successfully", file_path);
        log_file_write(log_content);
    }

    uint8_t ret = 0;
    uint8_t activate_num;
    uint8_t terminate_num;
    float lon_high;
    float lon_low;
    float lat_high;
    float lat_low;
    uint8_t direction;
    EVSP_touching_area_t *activate_area;
    for (int i = 0; i < EVSP_PHASE_MAX; i++) {
        ret = fscanf(fp, "%hhd", &activate_num);
        /* Check return value of fscanf */
        if (ret != 1) {
            log_file_write_fatal_error("EVSP_touching_area_plan_new: fscanf");
            perror("EVSP_touching_area_plan_new: fscanf");
            exit(errno);
        }
        for (int j = 0; j < activate_num; j++) {
            ret = fscanf(fp, "%f,%f,%f,%f %hhd", &lon_high, &lon_low, &lat_high,
                         &lat_low, &direction);
            /* Check return value of fscanf */
            if (ret != 5) {
                log_file_write_fatal_error(
                    "EVSP_touching_area_plan_new: fscanf");
                perror("EVSP_touching_area_plan_new: fscanf");
                exit(errno);
            }
            activate_area = EVSP_activate_touching_area_insert(
                &plan->list[i], lon_high, lon_low, lat_high, lat_low,
                direction);
            ret = fscanf(fp, "%hhd", &terminate_num);
            if (ret != 1) {
                log_file_write_fatal_error(
                    "EVSP_touching_area_plan_new: fscanf");
                perror("EVSP_touching_area_plan_new: fscanf");
                exit(errno);
            }
            for (int k = 0; k < terminate_num; k++) {
                ret = fscanf(fp, "%f,%f,%f,%f", &lon_high, &lon_low, &lat_high,
                             &lat_low);
                /* Check return value of fscanf */
                if (ret != 4) {
                    log_file_write_fatal_error(
                        "EVSP_touching_area_plan_new: fscanf");
                    perror("EVSP_touching_area_plan_new: fscanf");
                    exit(errno);
                }
                EVSP_terminate_touching_area_insert(activate_area, lon_high,
                                                    lon_low, lat_high, lat_low);
            }
        }
    }
    fclose(fp);
    return plan;
}

void EVSP_touching_area_plan_insert(char *file_name, uint8_t plan_id)
{
    EVSP_touching_area_plan_list_t *current;
    /* empty list */
    if (EVSP_touching_area_plan_list_head == NULL) {
        EVSP_touching_area_plan_list_head =
            EVSP_touching_area_plan_new(file_name, plan_id);
    } else {
        current = EVSP_touching_area_plan_list_head;
        /* traverse touching area plan list */
        while (current->next != NULL) {
            if (current->plan_id == plan_id) {
                return;
            }
            current = current->next;
        }
        current->next = EVSP_touching_area_plan_new(file_name, plan_id);
    }
}

EVSP_touching_area_plan_list_t *EVSP_touching_area_plan_search(uint8_t plan_id)
{
    EVSP_touching_area_plan_list_t *current = EVSP_touching_area_plan_list_head;

    /* traverse RSU matrix list */
    while (current != NULL) {
        if (current->plan_id == plan_id) {
            break;
        }
        current = current->next;
    }
    return current;
}

void EVSP_touching_area_plan_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP touching area plan list:");

    EVSP_touching_area_plan_list_t *current = EVSP_touching_area_plan_list_head;

    /* empty list */
    if (current == NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nempty");
        log_file_write(log_content);
        return;
    }

    /* traverse touching area plan list */
    while (current != NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nplan ID: %d",
                 current->plan_id);
        EVSP_touching_area_print(current);
        current = current->next;
    }
    log_file_write(log_content);
    return;
}

EVSP_touching_area_t *EVSP_touching_area_new(float lon_high,
                                             float lon_low,
                                             float lat_high,
                                             float lat_low)
{
    EVSP_touching_area_t *area =
        (EVSP_touching_area_t *) malloc(sizeof(EVSP_touching_area_t));
    if (area == NULL) {
        set_memory_error();
        log_file_write_fatal_error("EVSP_touching_area_new: malloc");
        perror("EVSP_touching_area_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(area, 0, sizeof(EVSP_touching_area_t));
    }
    area->lon_high = lon_high;
    area->lon_low = lon_low;
    area->lat_high = lat_high;
    area->lat_low = lat_low;
    area->terminate = NULL;
    area->next = NULL;
    return area;
}

EVSP_touching_area_t *EVSP_activate_touching_area_insert(
    EVSP_touching_area_t *list_head,
    float lon_high,
    float lon_low,
    float lat_high,
    float lat_low,
    uint8_t direction)
{
    EVSP_touching_area_t *current = list_head->next;
    /* empty list */
    if (current == NULL) {
        list_head->next =
            EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
        list_head->next->direciton = direction;
        return list_head->next;
    }

    /* traverse touching area list */
    while (current != NULL) {
        /* last node */
        if (current->next == NULL) {
            current->next =
                EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
            current->next->direciton = direction;
            return current->next;
        }
        current = current->next;
    }
    log_file_write_fatal_error("error inserting activate touching area");
    return NULL;
}

void EVSP_terminate_touching_area_insert(EVSP_touching_area_t *list_head,
                                         float lon_high,
                                         float lon_low,
                                         float lat_high,
                                         float lat_low)
{
    EVSP_touching_area_t *current = list_head->terminate;
    /* empty list */
    if (current == NULL) {
        list_head->terminate =
            EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
        return;
    }

    /* traverse touching area list */
    while (current != NULL) {
        /* last node */
        if (current->next == NULL) {
            current->next =
                EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
            return;
        }
        current = current->next;
    }
}

void EVSP_touching_area_print(EVSP_touching_area_plan_list_t *plan)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP touching area: PLAN (%d)", plan->plan_id);

    EVSP_touching_area_t *current;
    EVSP_touching_area_t *terminate;
    for (int i = 0; i < EVSP_PHASE_MAX; i++) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\nEVSP_touching_area_list[%d]:", i);
        current = plan->list[i].next;
        /* empty list */
        if (current == NULL) {
            continue;
        }

        /* traverse touching area list */
        while (current != NULL) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\n\t%f %f %f %f %d", current->lon_high, current->lon_low,
                     current->lat_high, current->lat_low, current->direciton);
            terminate = current->terminate;
            while (terminate != NULL) {
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\n\t\t%f %f %f %f", terminate->lon_high,
                         terminate->lon_low, terminate->lat_high,
                         terminate->lat_low);
                /* last node */
                if (terminate->next == NULL) {
                    break;
                }
                terminate = terminate->next;
            }
            /* last node */
            if (current->next == NULL) {
                break;
            }
            current = current->next;
        }
    }
    log_file_write(log_content);
    return;
}

EVSP_touching_area_t *EVSP_activate(float lon,
                                    float lat,
                                    uint8_t direction,
                                    uint8_t *target_phase,
                                    EVSP_touching_area_plan_list_t *plan)
{
    EVSP_touching_area_t *current;
    for (int i = 0; i < EVSP_PHASE_MAX; i++) {
        current = plan->list[i].next;
        /* empty list */
        if (current == NULL) {
            continue;
        }

        /* traverse touching area list */
        while (current != NULL) {
            if (lon < current->lon_high && lon > current->lon_low &&
                lat < current->lat_high && lat > current->lat_low &&
                direction == current->direciton) {
                *target_phase = i;
                return current;
            }
            /* last node */
            if (current->next == NULL) {
                break;
            }
            current = current->next;
        }
    }
    *target_phase = -1;
    return NULL;
}

bool EVSP_terminate(float lon, float lat, EVSP_touching_area_t *area_ptr)
{
    EVSP_touching_area_t *current;
    current = area_ptr->terminate;
    /* empty list */
    if (current == NULL) {
        log_file_write_fatal_error("terminate list should not be empty");
        return false;
    }

    /* traverse touching area list */
    while (current != NULL) {
        if (lon < current->lon_high && lon > current->lon_low &&
            lat < current->lat_high && lat > current->lat_low) {
            return true;
        }
        current = current->next;
    }
    return false;
}