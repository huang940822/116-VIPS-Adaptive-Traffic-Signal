#ifndef EVSP_TOUCHING_AREA_H
#define EVSP_TOUCHING_AREA_H

#include "EVSP_typedefine.h"

#define TOUCHING_AREA_DIR FILE_PATH "application/EVSP/config/touching_area/"
#define EVSP_CONFIG_DIR FILE_PATH "application/EVSP/config/"
#define TOUCHING_AREA_FILE "_touching_area.txt"

typedef enum area_type {
    TOUCHING_AREA = 0,
    TERMINATE_AREA = 1,
    TIMEOUT = 2,
} area_type_t;

extern EVSP_plan_list_t EVSP_plan_list;

int EVSP_default_config();
int EVSP_table_config();

EVSP_plan_table_t *EVSP_plan_table_search(uint8_t plan_id);

void EVSP_plan_list_print();

int EVSP_activate(float lon, float lat, uint8_t direction, EVSP_plan_table_t *plan, EVSP_touching_area_t **area_ptr);

EVSP_terminate_area_t *EVSP_terminate(float lon, float lat, EVSP_touching_area_t *area_ptr);
bool EVSP_plan_list_check();
void EVSP_plan_list_clean();

#endif