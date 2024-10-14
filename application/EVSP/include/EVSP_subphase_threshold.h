#ifndef EVSP_SUBPHASE_THRESHOLD_H
#define EVSP_SUBPHASE_THRESHOLD_H

#include "EVSP_typedefine.h"
#include "typedefine.h"

typedef struct EVSP_activate_OBU {
    char host_OBU_name[ID_MAX_LEN + 1];
    pthread_t activate_thread;
    pthread_mutex_t activate_mutex;
    int control_subphaseID;
    int target_phase;
    struct EVSP_activate_OBU *next;
    struct EVSP_activate_OBU *prev;
    int list_len;
} EVSP_activate_OBU_t;

int EVSP_extend_formula(int target_phase, traffic_signal_status_t *signal_status, int Tbf);
int EVSP_optimization(EVSP_host_OBU_obj_t *host_OBU, traffic_signal_status_t *signal_status, char *log_content);
int EVSP_OBU_activation_timer_start(EVSP_host_OBU_obj_t *host_OBU);
int EVSP_OBU_activation_time_end(char *OBU_name);
void init_activate_OBU_head();
EVSP_activate_OBU_t* search_activate_OBU(EVSP_host_OBU_obj_t *host_OBU);
void add_activate_OBU(EVSP_host_OBU_obj_t *host_OBU);
#endif