#ifndef ERROR_STATUS_H
#define ERROR_STATUS_H

#include <signal.h>

#define DSRC_BIT_POSITION 1
#define TSC_BIT_POSITION 2
#define DISK_BIT_POSITION 4
#define MEMORY_BIT_POSITION 8

uint8_t get_error_status();

extern timer_t dsrc_heartbeat_timer_id;


void set_dsrc_error(__sigval_t);
void clear_dsrc_error();

void set_tsc_error();
void clear_tsc_error();

void set_disk_error();
void clear_disk_error();

void set_memory_error();
void clear_memory_error();

void dsrc_error_detect_init(void);
// void set_DSRCerr_bit(void);
// void error_monitor(void);

#endif