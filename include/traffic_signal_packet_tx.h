#ifndef TRAFFIC_SIGNAL_PACKET_TX_H
#define TRAFFIC_SIGNAL_PACKET_TX_H

#include "typedefine.h"

#define SEQ_VAL 0x3B
#define DYNAMIC_LEN0_VAL 0x00
#define DYNAMIC_LEN1_VAL 0x0E
#define PRETIME_LEN0_VAL 0x00
#define PRETIME_LEN1_VAL 0x0E 
#define SWITCH_LEN0_VAL 0x00
#define SWITCH_LEN1_VAL 0x0F
#define EXTEND_LEN0_VAL 0x00
#define EXTEND_LEN1_VAL 0x0F
#define QUERY_LEN0_VAL 0x00
#define QUERY_LEN1_VAL 0x0C
#define QUERY_PLAN_LEN0_VAL 0x00
#define QUERY_PLAN_LEN1_VAL 0x0D
#define EVSP_LEN0_VAL 0x00
#define EVSP_LEN1_VAL 0x11

void tsc_dynamic();
void tsc_pretime();

void tsc_switch();
void tsc_extend(uint8_t subphase, uint8_t step, uint8_t effect_time);

void tsc_EVSP_on(uint8_t);
void tsc_EVSP_off();

void tsc_5F4C();
void tsc_5F48();
void tsc_5F44();
void tsc_5F45();

void tsc_countdown_on(uint8_t);
void tsc_countdown_off(uint8_t);
#endif