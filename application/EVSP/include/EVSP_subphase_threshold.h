#ifndef EVSP_SUBPHASE_THRESHOLD_H
#define EVSP_SUBPHASE_THRESHOLD_H

#include "EVSP_typedefine.h"
#include "typedefine.h"

int EVSP_extend_formula(int target_phase, traffic_signal_status_t *signal_status, int Tbf);
int EVSP_opptimiztion(int target_phase, EVSP_host_OBU_obj_t *host_OBU, traffic_signal_status_t *signal_status);

#endif