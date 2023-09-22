#ifndef TRAFFIC_COMPENSATION_H
#define TRAFFIC_COMPENSATION_H

#define COMPENSATION_NAME "COMPENSATION"
#define COMPENSATION_ID 99
#define COMPENSATION_priority 99
#define SUBPHASEID_NUM 8
#define COMPENSATION_CYCLE 2

void set_compensation_buffer(int subphaseId, int adjust_time);
void compensation_buffer_clear();
int16_t get_total_compensation_second();

uint8_t is_in_compensation();
void start_compensation();

#endif