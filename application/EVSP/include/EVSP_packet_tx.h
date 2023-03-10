#ifndef EVSP_PACKET_TX_H
#define EVSP_PACKET_TX_H
#include "EVSP_touching_area.h"
#include "EVSP_typedefine.h"

void EVSP_send_ack();
void EVSP_report_host_obu(OBU_object_t *OBU_object, uint8_t on_duty_flag);
void EVSP_report_activate_area(OBU_object_t *OBU_object, area_type_t type, int areaId);

#endif