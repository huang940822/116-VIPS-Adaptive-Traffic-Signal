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

int EVSP_plan_list_read()
{
    memset(&EVSP_plan_list, 0, sizeof(EVSP_plan_list_t));

    /* Get file path */
    char file_path[255];
    char rsu_name[RSU_NAME_MAX_LEN + 1];
    strncpy(rsu_name, config.RSU_name, RSU_NAME_MAX_LEN);
    char *name = trim_space(rsu_name);
    if (rsu_name == NULL) {
        log_file_write_fatal_error("error name %s", config.RSU_name);
    }

    memset(file_path, 0, sizeof(file_path));
    strcat(file_path, EVSP_CONFIG_DIR);
    strcat(file_path, rsu_name);
    strcat(file_path, TOUCHING_AREA_FILE);
    printf("%s\n", file_path);

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
                touch_area->direciton_end = (touch_area->direciton_end + 1) % 8; // + 1 不包含

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
    if (!EVSP_plan_list_check()) {
        goto EVSP_plan_list_read_error;
    }
    return 1;
EVSP_plan_list_read_error:
    EVSP_plan_list_clean();
    return -1;
}
#undef EVSP_touching_area_Vector_Increase

bool EVSP_plan_list_check()
{
    for (int i = 0; i < EVSP_plan_list.touching_area_count; i++) {
        for (int j = 0; j < EVSP_plan_list.touching_area[i].terminate_area_count; j++) {
            if (EVSP_plan_list.touching_area[i].terminate_area_Id[j] >= EVSP_plan_list.terminate_area_count) {
                return false;
            }
        }
    }
    /* plan_id 是唯一的 */
    uint8_t hash_table[256] = {0};
    for (int i = 0; i < EVSP_plan_list.plan_table_count; i++) {
        for (int j = 0; j < EVSP_plan_list.plan_table[i].plan_id_count; j++) {
            if (hash_table[EVSP_plan_list.plan_table[i].plan_id[j]] != 0) {
                return false;
            }
            hash_table[EVSP_plan_list.plan_table[i].plan_id[j]]++;
        }
        for (int j = 0; j < EVSP_plan_list.plan_table[i].plan_subPhase_count; j++) {
            for (int k = 0; k < EVSP_plan_list.plan_table[i].plan_subPhase[j].touching_area_count; k++) {
                if (EVSP_plan_list.plan_table[i].plan_subPhase[j].touching_area_Id[k] >= EVSP_plan_list.touching_area_count) {
                    return false;
                }
            }
        }
    }
    return true;
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
                    if (EVSP_plan_list.plan_table[i].plan_subPhase[j].touching_area_Id != NULL) {
                        free(EVSP_plan_list.plan_table[i].plan_subPhase[j].touching_area_Id);
                    }
                }
                free(EVSP_plan_list.plan_table[i].plan_subPhase);
            }
        }
        free(EVSP_plan_list.plan_table);
    }
    memset(&EVSP_plan_list, 0, sizeof(EVSP_plan_list_t));
}

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

    printf("%s\n", log_content);
    log_file_write(log_content);
    return;
}

bool onLine(const EVSP_Line_t *l1, const EVSP_Node_t *p)
{
    // Check whether p is on the line or not
    if (p->x <= MAX(l1->p1->x, l1->p2->x) && p->x <= MIN(l1->p1->x, l1->p2->x) &&
        (p->y <= MAX(l1->p1->y, l1->p2->y) && p->y <= MIN(l1->p1->y, l1->p2->y)))
        return true;
    return false;
}

int direction(const EVSP_Node_t *a, const EVSP_Node_t *b, const EVSP_Node_t *c)
{
    double val = (b->y - a->y) * (c->x - b->x) - (b->x - a->x) * (c->y - b->y);

    if (val == 0)
        // Colinear
        return 0;
    else if (val < 0)
        // Anti-clockwise direction
        return 2;
    // Clockwise direction
    return 1;
}

bool isIntersect(const EVSP_Line_t *l1, const EVSP_Line_t *l2)
{
    // Four direction for two lines and points of other line
    int dir1 = direction(l1->p1, l1->p2, l2->p1);
    int dir2 = direction(l1->p1, l1->p2, l2->p2);
    int dir3 = direction(l2->p1, l2->p2, l1->p1);
    int dir4 = direction(l2->p1, l2->p2, l1->p2);

    // When intersecting
    if (dir1 != dir2 && dir3 != dir4)
        return true;

    // When p2 of line2 are on the line1
    if (dir1 == 0 && onLine(l1, l2->p1))
        return true;

    // When p1 of line2 are on the line1
    if (dir2 == 0 && onLine(l1, l2->p2))
        return true;

    // When p2 of line1 are on the line2
    if (dir3 == 0 && onLine(l2, l1->p1))
        return true;

    // When p1 of line1 are on the line2
    if (dir4 == 0 && onLine(l2, l1->p2))
        return true;

    return false;
}

bool checkInside(EVSP_Node_t poly[], int n, EVSP_Node_t *p)
{
    // When polygon has less than 3 edge, it is not polygon
    if (n < 3)
        return false;

    // Create a point at infinity, y is same as point p
    EVSP_Line_t exline = (EVSP_Line_t){p, &(EVSP_Node_t){9999, p->y}};
    int count = 0;
    int i = 0;
    do {
        // Forming a line from two consecutive points of
        // poly
        EVSP_Line_t side = {&poly[i], &poly[(i + 1) % n]};
        if (isIntersect(&side, &exline)) {
            // If side is intersects exline
            if (direction(side.p1, p, side.p2) == 0)
                return onLine(&side, p);
            count++;
        }
        i = (i + 1) % n;
    } while (i != 0);

    // When count is odd
    return count & 1;
}

int EVSP_activate(float lon, float lat, uint8_t direction, EVSP_plan_table_t *plan, EVSP_touching_area_t **area_ptr)
{
    for (int i = 0; i < plan->plan_subPhase_count; ++i) {
        for (int j = 0; j < plan->plan_subPhase[i].touching_area_count; ++j) {
            EVSP_touching_area_t *touching_area = &EVSP_plan_list.touching_area[plan->plan_subPhase[i].touching_area_Id[j]];

            /* 判斷方向是否正確 */
            int k = touching_area->direciton_start;
            bool flag = false;
            do {
                if (k == direction) {
                    flag = true;
                    break;
                }
                k = (k + 1) % 8;
            } while (k != touching_area->direciton_end);

            if (!flag && checkInside(touching_area->node, touching_area->node_count, &(EVSP_Node_t){lon, lat})) {
                printf("EVSP_activate SubPhaseID %d\n", plan->plan_subPhase[i].SubPhaseID);
                *area_ptr = touching_area;
                return plan->plan_subPhase[i].SubPhaseID;
            }
        }
    }
    return -1;
}

bool EVSP_terminate(float lon, float lat, EVSP_touching_area_t *area_ptr)
{
    for (int i = 0; i < area_ptr->terminate_area_count; i++) {
        if (checkInside(EVSP_plan_list.terminate_area[area_ptr->terminate_area_Id[i]].node,
                EVSP_plan_list.terminate_area[area_ptr->terminate_area_Id[i]].node_count, &(EVSP_Node_t){lon, lat})) {
            printf("EVSP_terminate------------\n");
            return true;                                    
        }
    }
    return false;
}