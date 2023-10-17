#ifndef TRAFFIC_SIGNAL_STATUS_UPDATING_H
#define TRAFFIC_SIGNAL_STATUS_UPDATING_H

#include <pthread.h>
#include <semaphore.h>
#include "typedefine.h"

#define SEM_SIGNAL_STATUS_TIMEOUT 999


extern sem_t sem_signal_status;

void packet_5FCC(traffic_signal_packet_t *packet);
void packet_5FC8(traffic_signal_packet_t *packet);
void packet_5FC4(traffic_signal_packet_t *packet);
void packet_5FC5(traffic_signal_packet_t *packet);
void packet_5FC6(traffic_signal_packet_t *packet);

void packet_5F0C(traffic_signal_packet_t *packet);
void packet_5FC3(traffic_signal_packet_t *packet);
void packet_0F04(traffic_signal_packet_t *packet);
void packet_0FC2(traffic_signal_packet_t *packet);

void get_traffic_signal_status(traffic_signal_status_t *);
void get_current_traffic_signal_status(traffic_signal_status_t *);

uint8_t get_current_phase();
uint8_t get_current_step();
uint16_t get_current_second();
uint8_t get_SubPhaseCount();
uint8_t get_SignalCount();
uint16_t get_original_tc_health_status();

uint8_t get_plan_id();
uint8_t get_control_status();
uint8_t get_PhaseOrder();
uint16_t get_remaining_time(uint8_t phase, uint8_t step, uint16_t second);
uint8_t get_SignalStatus(uint8_t SubPhaseCount_index, uint8_t SignalCount_index);

void set_control_status(uint8_t control_status);
uint16_t get_original_tc_health_status();

void report_plan();

uint8_t get_next_SubPhaseID();
uint8_t get_prev_SubPhaseID();
// uint8_t get_next_StepID(uint8_t StepID);
// uint16_t get_next_StepSec(uint8_t SubPhaseID, uint8_t StepID);

void sem_timedwait_millsecs(sem_t *sem, long msecs);

#endif