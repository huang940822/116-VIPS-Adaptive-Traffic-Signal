#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TSP_matrix.h"
#include "TSP_typedefine.h"
#include "error_status.h"
#include "log.h"

TSP_RSU_matrix_t TSP_RSU_matrix_list;
TSP_OBU_matrix_t TSP_OBU_matrix_list;

TSP_RSU_matrix_t *TSP_RSU_matrix_new(char *file_name, uint8_t plan_id)
{
    TSP_RSU_matrix_t *matrix =
        (TSP_RSU_matrix_t *) malloc(sizeof(TSP_RSU_matrix_t));
    if (matrix == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_RSU_matrix_new: malloc");
        perror("TSP_RSU_matrix_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(matrix, 0, sizeof(TSP_RSU_matrix_t));
    }
    matrix->plan_id = plan_id;

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncpy(file_path, RSU_SUPERMATRIX_DIR, sizeof(RSU_SUPERMATRIX_DIR));
    strncat(file_path, file_name, sizeof(file_path));

    /* Open file */
    FILE *fp;
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", file_path);
        free(matrix);
        return NULL;
    } else {
        log_file_write("%s opened successfully", file_path);
    }

    char buf[255];
    int ret = 0;
    ret = fscanf(fp, "%s", buf);
    /* Check return value of fscanf */
    if (ret != 1) {
        log_file_write_fatal_error("TSP_RSU_matrix_new: fscanf");
        perror("TSP_RSU_matrix_new: fscanf");
        exit(errno);
    }
    /* store supermatrix */
    for (int i = 0; i < TSP_REMAINING_DISTANCE_NUM; i++) {
        for (int j = 0; j < TSP_SIGNAL_PHASE_NUM; j++) {
            for (int k = 0; k < TSP_REMAINING_TIME_NUM; k++) {
                for (int l = 0; l < TSP_TARGET_PHASE_NUM; l++) {
                    ret = fscanf(fp,
                                 "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,"
                                 "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,%hhd,",
                                 &matrix->entry[i][j][k][l].adjustment[0][0],
                                 &matrix->entry[i][j][k][l].adjustment[0][1],
                                 &matrix->entry[i][j][k][l].adjustment[0][2],
                                 &matrix->entry[i][j][k][l].adjustment[0][3],
                                 &matrix->entry[i][j][k][l].adjustment[0][4],
                                 &matrix->entry[i][j][k][l].adjustment[0][5],
                                 &matrix->entry[i][j][k][l].adjustment[0][6],
                                 &matrix->entry[i][j][k][l].adjustment[0][7],
                                 &matrix->entry[i][j][k][l].adjustment[1][0],
                                 &matrix->entry[i][j][k][l].adjustment[1][1],
                                 &matrix->entry[i][j][k][l].adjustment[1][2],
                                 &matrix->entry[i][j][k][l].adjustment[1][3],
                                 &matrix->entry[i][j][k][l].adjustment[1][4],
                                 &matrix->entry[i][j][k][l].adjustment[1][5],
                                 &matrix->entry[i][j][k][l].adjustment[1][6],
                                 &matrix->entry[i][j][k][l].adjustment[1][7]);
                    /* Check return value of fscanf */
                    if (ret != 16) {
                        log_file_write_fatal_error(
                            "TSP_RSU_matrix_new: fscanf");
                        perror("TSP_RSU_matrix_new: fscanf");
                        exit(errno);
                    }
                }
            }
        }
    }
    fclose(fp);
    return matrix;
}

TSP_RSU_matrix_t *TSP_RSU_matrix_search(uint8_t plan_id)
{
    TSP_RSU_matrix_t *current = TSP_RSU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        return NULL;
    }

    /* traverse RSU matrix list */
    while (current != NULL) {
        if (current->plan_id == plan_id) {
            return current;
        }
        /* last node */
        if (current->next == NULL) {
            return NULL;
        }
        current = current->next;
    }
    return NULL;
}

void TSP_RSU_matrix_insert(char *file_name, uint8_t plan_id)
{
    TSP_RSU_matrix_t *current = TSP_RSU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        TSP_RSU_matrix_list.next = TSP_RSU_matrix_new(file_name, plan_id);
        return;
    }

    /* traverse RSU matrix list */
    while (current != NULL) {
        if (current->plan_id == plan_id) {
            return;
        }

        /* last node */
        if (current->next == NULL) {
            current->next = TSP_RSU_matrix_new(file_name, plan_id);
            return;
        }
        current = current->next;
    }
}

void TSP_RSU_matrix_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "TSP RSU matrix:");

    TSP_RSU_matrix_t *current = TSP_RSU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nempty");
        log_file_write(log_content);
        return;
    }

    /* traverse RSU matrix list */
    while (current != NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nplan ID: %d",
                 current->plan_id);
        /* last node */
        if (current->next == NULL) {
            log_file_write(log_content);
            return;
        }
        current = current->next;
    }
    log_file_write(log_content);
    return;
}

TSP_OBU_matrix_t *TSP_OBU_matrix_new(char *file_name, uint8_t plan_id)
{
    TSP_OBU_matrix_t *matrix =
        (TSP_OBU_matrix_t *) malloc(sizeof(TSP_OBU_matrix_t));
    if (matrix == NULL) {
        set_memory_error();
        log_file_write_fatal_error("TSP_OBU_matrix_new: malloc");
        perror("TSP_OBU_matrix_new: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(matrix, 0, sizeof(TSP_OBU_matrix_t));
    }
    matrix->plan_id = plan_id;

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncpy(file_path, OBU_SUPERMATRIX_DIR, sizeof(OBU_SUPERMATRIX_DIR));
    strncat(file_path, file_name, sizeof(file_path));

    /* Open file */
    FILE *fp;
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s\n", file_path);
        free(matrix);
        return NULL;
    } else {
        log_file_write("%s opened successfully", file_path);
    }

    char buf[255];
    int ret = 0;
    ret = fscanf(fp, "%s", buf);
    /* Check return value of fscanf */
    if (ret != 1) {
        log_file_write_fatal_error("TSP_OBU_matrix_new: fscanf");
        perror("TSP_OBU_matrix_new: fscanf");
        exit(errno);
    }
    /* store supermatrix */
    for (int i = 0; i < TSP_REMAINING_DISTANCE_NUM; i++) {
        for (int j = 0; j < TSP_SIGNAL_PHASE_NUM; j++) {
            for (int k = 0; k < TSP_REMAINING_TIME_NUM; k++) {
                for (int l = 0; l < TSP_TARGET_PHASE_NUM; l++) {
                    ret = fscanf(fp, "%hhd,%hhd",
                                 &matrix->entry[i][j][k][l].recommend_speed,
                                 &matrix->entry[i][j][k][l].passing_rate);
                    /* Check return value of fscanf */
                    if (ret != 2) {
                        log_file_write_fatal_error(
                            "TSP_OBU_matrix_new: fscanf");
                        perror("TSP_OBU_matrix_new: fscanf");
                        exit(errno);
                    }
                }
            }
        }
    }
    fclose(fp);
    return matrix;
}

TSP_OBU_matrix_t *TSP_OBU_matrix_search(uint8_t plan_id)
{
    TSP_OBU_matrix_t *current = TSP_OBU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        return NULL;
    }

    /* traverse OBU matrix list */
    while (current != NULL) {
        if (current->plan_id == plan_id) {
            return current;
        }
        /* last node */
        if (current->next == NULL) {
            return NULL;
        }
        current = current->next;
    }
    return NULL;
}

void TSP_OBU_matrix_insert(char *file_name, uint8_t plan_id)
{
    TSP_OBU_matrix_t *current = TSP_OBU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        TSP_OBU_matrix_list.next = TSP_OBU_matrix_new(file_name, plan_id);
        return;
    }

    /* traverse OBU matrix list */
    while (current != NULL) {
        if (current->plan_id == plan_id) {
            return;
        }

        /* last node */
        if (current->next == NULL) {
            current->next = TSP_OBU_matrix_new(file_name, plan_id);
            return;
        }
        current = current->next;
    }
}

void TSP_OBU_matrix_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "TSP OBU matrix:");

    TSP_OBU_matrix_t *current = TSP_OBU_matrix_list.next;

    /* empty list */
    if (current == NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nempty");
        log_file_write(log_content);
        return;
    }

    /* traverse OBU matrix list */
    while (current != NULL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nplan ID: %d",
                 current->plan_id);
        /* last node */
        if (current->next == NULL) {
            log_file_write(log_content);
            return;
        }
        current = current->next;
    }
}