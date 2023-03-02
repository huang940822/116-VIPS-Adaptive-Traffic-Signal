#ifndef TRAFFIC_COMPENSATION_H
#define TRAFFIC_COMPENSATION_H

#define COMPENSATION_NAME "COMPENSATION"
#define COMPENSATION_LEN 14
#define COMPENSATION_ID 99
#define COMPENSATION_priority 99
#define SUBPHASEID_NUM 8
#define COMPENSATION_CYCLE 2

int16_t compensation_buffer[SUBPHASEID_NUM];

void compensation_buffer_clear();
uint8_t is_in_compensation();
void traffic_compensation_method1(uint8_t compensation_cyclenum);
void traffic_compensation_method2(uint8_t compensation_cyclenum,
                                  float phase_weight[PHASE_COUNT_MAX_NUM]);
void traffic_compensation_method3(uint8_t compensation_cyclenum);

#endif