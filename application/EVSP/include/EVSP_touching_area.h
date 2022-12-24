#ifndef EVSP_TOUCHING_AREA_H
#define EVSP_TOUCHING_AREA_H

#include "EVSP_typedefine.h"

#define TOUCHING_AREA_DIR FILE_PATH "application/EVSP/config/touching_area/"

extern EVSP_plan_list_t EVSP_plan_list;

int EVSP_plan_list_read(char *file_name);
// EVSP_touching_area_plan_list_t *EVSP_touching_area_plan_new(char *file_name,
//                                                             uint8_t plan_id);
EVSP_plan_table_t *EVSP_plan_table_search(uint8_t plan_id);
// void EVSP_touching_area_plan_insert(char *file_name, uint8_t plan_id);
void EVSP_plan_list_print();

// EVSP_touching_area_t *EVSP_touching_area_new(float lon_high,
//                                              float lon_low,
//                                              float lat_high,
//                                              float lat_low);
// EVSP_touching_area_t *EVSP_activate_touching_area_insert(
//     EVSP_touching_area_t *list_head,
//     float lon_high,
//     float lon_low,
//     float lat_high,
//     float lat_low,
//     uint8_t direction);
// void EVSP_terminate_touching_area_insert(EVSP_touching_area_t *list_head,
//                                          float lon_high,
//                                          float lon_low,
//                                          float lat_high,
//                                          float lat_low);
// void EVSP_touching_area_print();

int EVSP_activate(float lon, float lat, uint8_t direction, EVSP_plan_table_t *plan, EVSP_touching_area_t **area_ptr);

bool EVSP_terminate(float lon, float lat, EVSP_touching_area_t *area_ptr);
bool EVSP_plan_list_check();
void EVSP_plan_list_clean();

#endif