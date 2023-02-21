#ifndef TSP_PACKET_TX_H
#define TSP_PACKET_TX_H

#include "TSP_typedefine.h"

void TSP_send_ack(uint8_t cmd, uint8_t status);
void TSP_report_plan();
void TSP_report_command(uint8_t control_status,
                        uint8_t sub_phase_id,
                        uint8_t step_id,
                        uint8_t effect_time,
                        char *OBU_name);
void TSP_OBU_boardcast(TSP_host_OBU_obj_t *host_OBU);

#endif