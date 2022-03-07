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
#define FIRMQ_LEN0 0x00
#define FIRMQ_LEN1 0x0C

pthread_mutex_t mutex_rs232_write;

uint8_t tsc_dynamic();
uint8_t tsc_pretime();

uint8_t tsc_switch();
uint8_t tsc_extend(uint8_t subphase, uint8_t step, uint8_t effect_time);

// void tsc_EVSP_on(uint8_t);
// void tsc_EVSP_off();

uint8_t tsc_5F4C();
uint8_t tsc_5F48();
uint8_t tsc_5F44();
uint8_t tsc_5F45();

uint8_t tsc_countdown_on(uint8_t);
uint8_t tsc_countdown_off(uint8_t);
uint8_t tsc_query_firmware_version(void);
#endif