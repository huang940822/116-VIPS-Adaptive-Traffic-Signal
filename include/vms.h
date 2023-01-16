#ifndef VMS_H
#define VMS_H

#include <signal.h>
#include "time.h"
#include <pthread.h>
#include <stdint.h>

#define POLLING_INTERVAL 0.5
#define RTM_MAX 6
#define TM_MAX 6 
#define NO_SHOW 0xFFFF
#define TM_NO_SHOW 0xFF

extern pthread_mutex_t sec_mutex;
uint16_t n_rtm_sec[RTM_MAX];
uint8_t n_tm_sec[TM_MAX];

void vms_handler_init();

#endif