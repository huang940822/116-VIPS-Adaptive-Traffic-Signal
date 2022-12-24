#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EVSP_touching_area.h"
#include "config.h"
#include "error_status.h"
#include "log.h"

EVSP_plan_list_t EVSP_plan_list = {0};

bool static inline read_node(char *buf, EVSP_Node_t *node)
{
    char *tmp = buf;
    buf = strsep(&tmp, " ");
    buf = trim_space(buf);
    tmp = trim_space(tmp);
    
    if (buf == NULL || tmp == NULL)
        return false;
    if (sscanf(buf, "%lf", &node->lon) != 1 || sscanf(tmp, "%lf", &node->lat) != 1)
        return false;
    if (-180 <= node->lon && node->lon < 180 && -90 <= node->lat && node->lat <= 90)
        return true;
    return false;
}

/* 如果 vector 的記憶體空間不夠的話 一次都多 5 個 */
/* 這裡會需要一些命名規則才可以使用 */
/* vector_name: 陣列的變數名稱 */
/* vector_name_max: 最大值的變數名稱 */
/* EVSP_vector_name_t: 陣列的型態名稱 */
#define EVSP_plan_list_Vector_Increase(p_name, dot, vector_name)                                      \
    do {                                                                                              \
        if (p_name dot vector_name##_count == vector_name##_max) {                                    \
            vector_name##_max += 5;                                                                   \
            Realloc(p_name dot vector_name, sizeof(EVSP_##vector_name##_t) * (vector_name##_max - 5), \
                    sizeof(EVSP_##vector_name##_t) * vector_name##_max, "EVSP_" #vector_name "_new"); \
        }                                                                                             \
    } while (0)

int EVSP_plan_list_read(char *file_name)
{
    memset(&EVSP_plan_list, 0, sizeof(EVSP_plan_list_t));

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncat(file_path, file_name, sizeof(file_path));

    FILE *fp;
    fp = fopen(file_path, "r");
    if (fp == NULL) {
        log_file_write_fatal_error("error opening %s", file_path);
        return -1;
    } else {
        log_file_write("%s opened successfully", file_path);
    }

    int plan_table_max = 0;
    char read_buf[CONFIG_LINE_BUFFER_SIZE];
    while (!feof(fp)) {
        char *buf = read_line(read_buf, sizeof(read_buf), fp);
        if (buf == NULL)
            continue;
        if (strstr(buf, "terminate_area_table")) {
            int terminate_area_max = 0;
            EVSP_plan_list.terminate_area_count = 0;

            while (true) {
                if (feof(fp))
                    goto EVSP_plan_list_read_error;

                char *buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    continue;
                if (strstr(buf, "terminate_area_table_end")) {
                    break;
                }

                uint8_t uint8_t_val;
                /* id */
                char *sepstr = buf;
                char *substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                    goto EVSP_plan_list_read_error;
                }
                if (uint8_t_val != EVSP_plan_list.terminate_area_count) {
                    goto EVSP_plan_list_read_error;
                }

                /* 如果 terminate_area 的記憶體空間不夠的話 一次都多五個*/
                EVSP_plan_list_Vector_Increase(EVSP_plan_list, ., terminate_area);
                EVSP_terminate_area_t *term_area = &EVSP_plan_list.terminate_area[EVSP_plan_list.terminate_area_count];
                EVSP_plan_list.terminate_area_count++;

                /* node_count */
                substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                    goto EVSP_plan_list_read_error;
                }
                if (uint8_t_val > MAX_NODE_COUNT) {
                    goto EVSP_plan_list_read_error;
                }

                term_area->node_count = uint8_t_val;

                Malloc(term_area->node, sizeof(EVSP_Node_t) * uint8_t_val, "EVSP_Node_new");

                for (int i = 0; i < term_area->node_count; ++i) {
                    
                    if (sepstr == NULL)
                        goto EVSP_plan_list_read_error;
                    substr = trim_space(strsep(&sepstr, ","));
                    if (substr == NULL)
                        goto EVSP_plan_list_read_error;

                    if (read_node(substr, &term_area->node[i]) == false)
                        goto EVSP_plan_list_read_error;
                }
            }
        }

        if (strncmp(buf, "touching_area_table", sizeof("touching_area_table") - 1) == 0) {
            int touching_area_max = 0;
            EVSP_plan_list.touching_area_count = 0;

            while (true) {
                if (feof(fp))
                    goto EVSP_plan_list_read_error;

                char *buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    continue;
                if (strstr(buf, "touching_area_table_end")) {
                    break;
                }

                uint8_t uint8_t_val;
                /* id */
                char *sepstr = buf;
                char *substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                    goto EVSP_plan_list_read_error;
                }
                if (uint8_t_val != EVSP_plan_list.touching_area_count) {
                    goto EVSP_plan_list_read_error;
                }

                /* 如果 touching_area 的記憶體空間不夠的話 一次都多五個*/
                EVSP_plan_list_Vector_Increase(EVSP_plan_list, ., touching_area);
                EVSP_touching_area_t *touch_area = &EVSP_plan_list.touching_area[EVSP_plan_list.touching_area_count];
                EVSP_plan_list.touching_area_count++;

                /* direction */
                substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL)
                    goto EVSP_plan_list_read_error;

                char *tmp = substr;
                substr = strsep(&tmp, " ");
                if (substr == NULL || tmp == NULL)
                    return false;
                if (sscanf(substr, "%hhd", &touch_area->direciton_start) != 1 ||
                    sscanf(tmp, "%hhd", &touch_area->direciton_end) != 1)
                    return false;
                if (touch_area->direciton_start > 7 || touch_area->direciton_end > 7)
                    goto EVSP_plan_list_read_error;

                /* node_count */
                substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    goto EVSP_plan_list_read_error;
                if (uint8_t_val > MAX_NODE_COUNT) {
                    goto EVSP_plan_list_read_error;
                }
                touch_area->node_count = uint8_t_val;
                Malloc(touch_area->node, sizeof(EVSP_Node_t) * uint8_t_val, "EVSP_Node_new");

                for (int i = 0; i < touch_area->node_count; ++i) {
                    substr = trim_space(strsep(&sepstr, ","));
                    if (substr == NULL || sepstr == NULL)
                        goto EVSP_plan_list_read_error;
                    if (read_node(substr, &touch_area->node[i]) == false)
                        goto EVSP_plan_list_read_error;
                }

                /* terminate_area_count */
                substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    goto EVSP_plan_list_read_error;
                if (uint8_t_val > MAX_NODE_COUNT) {  // 只是設個避免填錯的上限 之後會檢查
                    goto EVSP_plan_list_read_error;
                }
                touch_area->terminate_area_count = uint8_t_val;
                Malloc(touch_area->terminate_area_Id, sizeof(uint8_t) * uint8_t_val, "EVSP_terminate_area_Id_new");

                for (int i = 0; i < touch_area->terminate_area_count; ++i) {
                    if (sepstr == NULL)
                        goto EVSP_plan_list_read_error;
                    substr = trim_space(strsep(&sepstr, ","));
                    if (substr == NULL || sscanf(substr, "%hhd", &touch_area->terminate_area_Id[i]) != 1)
                        goto EVSP_plan_list_read_error;
                }
            }
        }
        if (strncmp(buf, "plan_table", sizeof("plan_table") - 1) == 0) {
            EVSP_plan_list_Vector_Increase(EVSP_plan_list, ., plan_table);
            EVSP_plan_table_t *plan_table = &EVSP_plan_list.plan_table[EVSP_plan_list.plan_table_count];
            EVSP_plan_list.plan_table_count++;

            uint8_t uint8_t_val;
            int plan_id_max = 0;
            char *substr = trim_space(buf + sizeof("plan_table") - 1);
            char *save_ptr = NULL;
            substr = strtok_r(substr, ",", &save_ptr);
            do {
                substr = trim_space(substr);
                if (substr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1)
                    goto EVSP_plan_list_read_error;

#define EVSP_plan_id_t uint8_t
                EVSP_plan_list_Vector_Increase(plan_table, ->, plan_id);
#undef EVSP_plan_id_t

                plan_table->plan_id[plan_table->plan_id_count] = uint8_t_val;
                plan_table->plan_id_count++;

                substr = strtok_r(NULL, ",", &save_ptr);
            } while (substr);

            int plan_subPhase_max = 0;
            while (true) {
                if (feof(fp))
                    goto EVSP_plan_list_read_error;

                char *buf = read_line(read_buf, sizeof(read_buf), fp);
                if (buf == NULL)
                    continue;
                if (strstr(buf, "plan_table_end"))
                    break;

                EVSP_plan_list_Vector_Increase(plan_table, ->, plan_subPhase);
                EVSP_plan_subPhase_t *subPhase = &plan_table->plan_subPhase[plan_table->plan_subPhase_count];
                plan_table->plan_subPhase_count++;

                uint8_t uint8_t_val;
                /* SubphaseId */
                char *sepstr = buf;
                char *substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                    goto EVSP_plan_list_read_error;
                }
                if (uint8_t_val > EVSP_PHASE_MAX) {
                    goto EVSP_plan_list_read_error;
                }
                subPhase->SubPhaseID = uint8_t_val;

                substr = trim_space(strsep(&sepstr, ","));
                if (substr == NULL || sepstr == NULL || sscanf(substr, "%hhd", &uint8_t_val) != 1) {
                    goto EVSP_plan_list_read_error;
                }
                if (uint8_t_val > MAX_NODE_COUNT) {  // 只是設個避免填錯的上限 之後會檢查
                    goto EVSP_plan_list_read_error;
                }

                subPhase->touching_area_count = uint8_t_val;
                Malloc(subPhase->touching_area_Id, sizeof(uint8_t) * uint8_t_val, "EVSP_touching_area_Id_new");

                for (int i = 0; i < subPhase->touching_area_count; ++i) {
                    if (sepstr == NULL)
                        goto EVSP_plan_list_read_error;
                    substr = trim_space(strsep(&sepstr, ","));
                    if (substr == NULL || sscanf(substr, "%hhd", &subPhase->touching_area_Id[i]) != 1)
                        goto EVSP_plan_list_read_error;
                }
            }
        }
    }


    return 1;
EVSP_plan_list_read_error:
    EVSP_plan_list_clean();
    return -1;
}
#undef EVSP_touching_area_Vector_Increase

bool EVSP_plan_list_check()
{
}

/* 清除 EVSP_plan_list */
void EVSP_plan_list_clean()
{
    if (EVSP_plan_list.terminate_area_count != 0 && EVSP_plan_list.terminate_area != NULL) {
        for (int i = 0; i < EVSP_plan_list.terminate_area_count; i++) {
            if (EVSP_plan_list.terminate_area[i].node != NULL) {
                free(EVSP_plan_list.terminate_area[i].node);
            }
        }
        free(EVSP_plan_list.terminate_area);
    }
    if (EVSP_plan_list.touching_area_count != 0 && EVSP_plan_list.touching_area != NULL) {
        for (int i = 0; i < EVSP_plan_list.touching_area_count; i++) {
            if (EVSP_plan_list.touching_area[i].node != NULL) {
                free(EVSP_plan_list.touching_area[i].node);
            }
            if (EVSP_plan_list.touching_area[i].terminate_area_Id != NULL) {
                free(EVSP_plan_list.touching_area[i].terminate_area_Id);
            }
        }
        free(EVSP_plan_list.touching_area);
    }
    if (EVSP_plan_list.plan_table_count != 0 && EVSP_plan_list.plan_table != NULL) {
        for (int i = 0; i < EVSP_plan_list.plan_table_count; i++) {
            if (EVSP_plan_list.plan_table[i].plan_subPhase_count != 0 &&
                EVSP_plan_list.plan_table[i].plan_subPhase != NULL) {
                for (int j = 0; j < EVSP_plan_list.plan_table[i].plan_subPhase_count; j++) {
                    if (EVSP_plan_list.plan_table[i].plan_subPhase->touching_area_Id != NULL) {
                        free(EVSP_plan_list.plan_table[i].plan_subPhase->touching_area_Id);
                    }
                }
                free(EVSP_plan_list.plan_table[i].plan_subPhase);
            }
        }
        free(EVSP_plan_list.plan_table);
    }
    memset(&EVSP_plan_list, 0, sizeof(EVSP_plan_list_t));
}

// EVSP_touching_area_plan_list_t *EVSP_touching_area_plan_new(char *file_name,
//                                                             uint8_t plan_id)
// {

//     EVSP_touching_area_plan_list_t *plan;
//     Malloc(plan, sizeof(EVSP_touching_area_plan_list_t), "EVSP_touching_area_plan_new");

//     plan->plan_id = plan_id;

//     /* Get file path */
//     char file_path[255];
//     memset(file_path, 0, sizeof(file_path));
//     strncpy(file_path, TOUCHING_AREA_DIR, sizeof(TOUCHING_AREA_DIR));
//     strncat(file_path, file_name, sizeof(file_path));

//     /* Open file */
//     FILE *fp;
//     fp = fopen(file_path, "r");
//     if (fp == NULL) {
//         log_file_write_fatal_error("error opening %s", file_path);
//         return NULL;
//     } else {
//         log_file_write("%s opened successfully", file_path);
//     }



//     uint8_t ret = 0;
//     uint8_t activate_num;
//     uint8_t terminate_num;
//     float lon_high;
//     float lon_low;
//     float lat_high;
//     float lat_low;
//     uint8_t direction;
//     EVSP_touching_area_t *activate_area;
//     for (int i = 0; i < EVSP_PHASE_MAX; i++) {
//         ret = fscanf(fp, "%hhd", &activate_num);
//         /* Check return value of fscanf */
//         if (ret != 1) {
//             log_file_write_fatal_error("EVSP_touching_area_plan_new: fscanf");
//             perror("EVSP_touching_area_plan_new: fscanf");
//             exit(errno);
//         }
//         for (int j = 0; j < activate_num; j++) {
//             ret = fscanf(fp, "%f,%f,%f,%f %hhd", &lon_high, &lon_low, &lat_high,
//                          &lat_low, &direction);
//             /* Check return value of fscanf */
//             if (ret != 5) {
//                 log_file_write_fatal_error(
//                     "EVSP_touching_area_plan_new: fscanf");
//                 perror("EVSP_touching_area_plan_new: fscanf");
//                 exit(errno);
//             }
//             activate_area = EVSP_activate_touching_area_insert(
//                 &plan->list[i], lon_high, lon_low, lat_high, lat_low,
//                 direction);
//             ret = fscanf(fp, "%hhd", &terminate_num);
//             if (ret != 1) {
//                 log_file_write_fatal_error(
//                     "EVSP_touching_area_plan_new: fscanf");
//                 perror("EVSP_touching_area_plan_new: fscanf");
//                 exit(errno);
//             }
//             for (int k = 0; k < terminate_num; k++) {
//                 ret = fscanf(fp, "%f,%f,%f,%f", &lon_high, &lon_low, &lat_high,
//                              &lat_low);
//                 /* Check return value of fscanf */
//                 if (ret != 4) {
//                     log_file_write_fatal_error(
//                         "EVSP_touching_area_plan_new: fscanf");
//                     perror("EVSP_touching_area_plan_new: fscanf");
//                     exit(errno);
//                 }
//                 EVSP_terminate_touching_area_insert(activate_area, lon_high,
//                                                     lon_low, lat_high, lat_low);
//             }
//         }
//     }
//     fclose(fp);
//     return plan;
// }

// void EVSP_touching_area_plan_insert(char *file_name, uint8_t plan_id)
// {
//     EVSP_touching_area_plan_list_t *current;
//     /* empty list */
//     if (EVSP_touching_area_plan_list_head == NULL) {
//         EVSP_touching_area_plan_list_head =
//             EVSP_touching_area_plan_new(file_name, plan_id);
//     } else {
//         current = EVSP_touching_area_plan_list_head;
//         /* traverse touching area plan list */
//         while (current->next != NULL) {
//             if (current->plan_id == plan_id) {
//                 return;
//             }
//             current = current->next;
//         }
//         current->next = EVSP_touching_area_plan_new(file_name, plan_id);
//     }
// }

/* 使用 plan id 尋找相應的 plan table */
EVSP_plan_table_t *EVSP_plan_table_search(uint8_t plan_id)
{
    for (int i = 0; i < EVSP_plan_list.plan_table_count; i++) {
        for (int j = 0; j < EVSP_plan_list.plan_table[i].plan_id_count; j++) {
            if (EVSP_plan_list.plan_table[i].plan_id[j] == plan_id)
                return &EVSP_plan_list.plan_table[i];
        }
    }
    return NULL;
}

void EVSP_plan_list_print()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP touching area plan list:");

    snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\nterminate_area_count %d\n",
                 EVSP_plan_list.terminate_area_count);
    for (int i = 0; i < EVSP_plan_list.terminate_area_count; i++) {
        for (int j = 0; j < EVSP_plan_list.terminate_area[i].node_count; j++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), " %lf %lf,",
                     EVSP_plan_list.terminate_area[i].node->lon, EVSP_plan_list.terminate_area[i].node->lat);
        }
    }
    snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\ntouching_area_count %d\n",
                 EVSP_plan_list.touching_area_count);
    for (int i = 0; i < EVSP_plan_list.touching_area_count; i++) {
        
        for (int j = 0; j < EVSP_plan_list.touching_area[i].node_count; j++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), " %lf %lf,",
                     EVSP_plan_list.touching_area[i].node->lon, EVSP_plan_list.touching_area[i].node->lat);
        }
    }
    // EVSP_touching_area_plan_list_t *current = EVSP_touching_area_plan_list_head;

    // /* empty list */
    // if (current == NULL) {
    //     snprintf(log_content + strlen(log_content),
    //              LOG_CONTENT_LEN - strlen(log_content), "\nempty");
    //     log_file_write(log_content);
    //     return;
    // }

    // /* traverse touching area plan list */
    // while (current != NULL) {
    //     snprintf(log_content + strlen(log_content),
    //              LOG_CONTENT_LEN - strlen(log_content), "\nplan ID: %d",
    //              current->plan_id);
    //     EVSP_touching_area_print(current);
    //     current = current->next;
    // }
    printf("%s\n", log_content);
    log_file_write(log_content);
    return;
}

// EVSP_touching_area_t *EVSP_touching_area_new(float lon_high,
//                                              float lon_low,
//                                              float lat_high,
//                                              float lat_low)
// {
//     EVSP_touching_area_t *area;
//     Malloc(area, sizeof(EVSP_touching_area_t), "EVSP_touching_area_new");

//     area->lon_high = lon_high;
//     area->lon_low = lon_low;
//     area->lat_high = lat_high;
//     area->lat_low = lat_low;
//     area->terminate = NULL;
//     area->next = NULL;
//     return area;
// }

// EVSP_touching_area_t *EVSP_activate_touching_area_insert(
//     EVSP_touching_area_t *list_head,
//     float lon_high,
//     float lon_low,
//     float lat_high,
//     float lat_low,
//     uint8_t direction)
// {
//     EVSP_touching_area_t *current = list_head->next;
//     /* empty list */
//     if (current == NULL) {
//         list_head->next =
//             EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
//         list_head->next->direciton = direction;
//         return list_head->next;
//     }

//     /* traverse touching area list */
//     while (current != NULL) {
//         /* last node */
//         if (current->next == NULL) {
//             current->next =
//                 EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
//             current->next->direciton = direction;
//             return current->next;
//         }
//         current = current->next;
//     }
//     log_file_write_fatal_error("error inserting activate touching area");
//     return NULL;
// }

// void EVSP_terminate_touching_area_insert(EVSP_touching_area_t *list_head,
//                                          float lon_high,
//                                          float lon_low,
//                                          float lat_high,
//                                          float lat_low)
// {
//     EVSP_touching_area_t *current = list_head->terminate;
//     /* empty list */
//     if (current == NULL) {
//         list_head->terminate =
//             EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
//         return;
//     }

//     /* traverse touching area list */
//     while (current != NULL) {
//         /* last node */
//         if (current->next == NULL) {
//             current->next =
//                 EVSP_touching_area_new(lon_high, lon_low, lat_high, lat_low);
//             return;
//         }
//         current = current->next;
//     }
// }

// void EVSP_touching_area_print(EVSP_touching_area_plan_list_t *plan)
// {
//     char log_content[LOG_CONTENT_LEN + 1];
//     memset(log_content, 0, sizeof(log_content));
//     snprintf(log_content + strlen(log_content),
//              LOG_CONTENT_LEN - strlen(log_content),
//              "EVSP touching area: PLAN (%d)", plan->plan_id);

//     EVSP_touching_area_t *current;
//     EVSP_touching_area_t *terminate;
//     for (int i = 0; i < EVSP_PHASE_MAX; i++) {
//         snprintf(log_content + strlen(log_content),
//                  LOG_CONTENT_LEN - strlen(log_content),
//                  "\nEVSP_touching_area_list[%d]:", i);
//         current = plan->list[i].next;
//         /* empty list */
//         if (current == NULL) {
//             continue;
//         }

//         /* traverse touching area list */
//         while (current != NULL) {
//             snprintf(log_content + strlen(log_content),
//                      LOG_CONTENT_LEN - strlen(log_content),
//                      "\n\t%f %f %f %f %d", current->lon_high, current->lon_low,
//                      current->lat_high, current->lat_low, current->direciton);
//             terminate = current->terminate;
//             while (terminate != NULL) {
//                 snprintf(log_content + strlen(log_content),
//                          LOG_CONTENT_LEN - strlen(log_content),
//                          "\n\t\t%f %f %f %f", terminate->lon_high,
//                          terminate->lon_low, terminate->lat_high,
//                          terminate->lat_low);
//                 /* last node */
//                 if (terminate->next == NULL) {
//                     break;
//                 }
//                 terminate = terminate->next;
//             }
//             /* last node */
//             if (current->next == NULL) {
//                 break;
//             }
//             current = current->next;
//         }
//     }
//     log_file_write(log_content);
//     return;
// }


int EVSP_activate(float lon, float lat, uint8_t direction, EVSP_plan_table_t *plan, EVSP_touching_area_t **area_ptr)
{
    for (int i = 0; i < plan->plan_subPhase_count; ++i) {
        for (int j = 0; j < plan->plan_subPhase[i].touching_area_count; ++j) {
            EVSP_plan_list.touching_area[plan->plan_subPhase[i].touching_area_Id[j]];
        }
    }
    return -1;
}

bool EVSP_terminate(float lon, float lat, EVSP_touching_area_t *area_ptr)
{
    // EVSP_touching_area_t *current;
    // current = area_ptr->terminate;
    // /* empty list */
    // if (current == NULL) {
    //     log_file_write_fatal_error("terminate list should not be empty");
    //     return false;
    // }

    // /* traverse touching area list */
    // while (current != NULL) {
    //     if (lon < current->lon_high && lon > current->lon_low &&
    //         lat < current->lat_high && lat > current->lat_low) {
    //         return true;
    //     }
    //     current = current->next;
    // }
    return false;
}