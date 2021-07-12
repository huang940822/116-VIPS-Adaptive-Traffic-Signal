#ifndef TSP_MATRIX_H
#define TSP_MATRIX_H

#include "TSP_typedefine.h"

#define RSU_SUPERMATRIX_DIR "./application/TSP/config/RSU_supermatrix/"
#define OBU_SUPERMATRIX_DIR "./application/TSP/config/OBU_supermatrix/"

extern TSP_RSU_matrix_t TSP_RSU_matrix_list;
extern TSP_OBU_matrix_t TSP_OBU_matrix_list;

TSP_RSU_matrix_t *TSP_RSU_matrix_new(char *file_name, uint8_t plan_id);
TSP_RSU_matrix_t *TSP_RSU_matrix_search(uint8_t plan_id);
void TSP_RSU_matrix_insert(char *file_name, uint8_t plan_id);
void TSP_RSU_matrix_print();

TSP_OBU_matrix_t *TSP_OBU_matrix_new(char *file_name, uint8_t plan_id);
TSP_OBU_matrix_t *TSP_OBU_matrix_search(uint8_t plan_id);
void TSP_OBU_matrix_insert(char *file_name, uint8_t plan_id);
void TSP_OBU_matrix_print();

#endif