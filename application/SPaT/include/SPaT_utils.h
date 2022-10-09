#ifndef SPAT_UTILS_H
#define SPAT_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "j2735_codec.h"
#include "SPaT_config.h"
#include "traffic_signal_status_updating.h"

#define MAX_NUM_SIGNAL_GROUP 2
#define ERR_MSG_SZ 128
#define to_TimeMark(tmark) \
    (((tmark).min * 60 * 10) + ((tmark).sec * 10) + ((tmark).ms / 100))

extern SPaT_config_object_t SPaT_config;
/* A fake signal phase and time */

extern struct SPaT_config_intersection_info intersection_info;

typedef enum {
    SIGNAL_INVALID,
    /* mapping to state listed in J2735 */
    SIGNAL_GREEN = MovementPhaseState_permissive_Movement_Allowed,
    /* mapping to state listed in J2735 */
    SIGNAL_YELLOW = MovementPhaseState_protected_clearance,
    /* mapping to state listed in J2735 */
    SIGNAL_RED = MovementPhaseState_stop_And_Remain,
} signal_t;

/* TimeMark: minute, second and ms of UTC. */
typedef struct timemark {
    unsigned int min;
    unsigned int sec;
    unsigned int ms;
} timemark_t;

struct tc_now_time {
    int8_t Sec, Min, Hour, Day, Month, Year;
};
time_t compare_to_tc_time;

bool compare_time(const traffic_signal_status_t * const signal_status);
void dump_mem(void *data, int len);
int spat_msg_init(SPAT **pp_spat);
int compose_spat(uint8_t **pp_spat_buf, SPAT *p_spat);
int spat_msg_update(SPAT **pp_spat);
void print_spat(SPAT **pp_spat);
int leapYear(int a);
int calDate(int year, int month, int day);
#endif
