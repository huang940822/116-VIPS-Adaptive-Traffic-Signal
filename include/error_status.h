#ifndef ERROR_STATUS_H
#define ERROR_STATUS_H

#include <signal.h>
#include "typedefine.h"

#define DSRC_BIT_POSITION 1
#define TSC_BIT_POSITION 2
#define DISK_BIT_POSITION 4
#define MEMORY_BIT_POSITION 8
#define TCFAIL_BIT_POSITION 16
#define TC_655XX_ERR 32
#define CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR 64

uint8_t get_error_status();
extern timer_t dsrc_heartbeat_timer_id;

void set_tsc_error();
void clear_tsc_error();

void set_disk_error();
void clear_disk_error();

void set_memory_error();
void clear_memory_error();

void dsrc_error_detect_init(void);
void set_dsrc_error(__sigval_t);
void clear_dsrc_error();

void tc_5fcc_error_detect_init(void);
void clear_tsc_5fcc_error(void);
void set_tsc_5fcc_error(__sigval_t value);

void set_655xx_error(void);
void clear_655xx_error(void);

void set_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR(void);
void clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR(void);
#endif