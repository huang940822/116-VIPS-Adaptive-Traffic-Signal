#ifndef TIB_SPAT_UTILS_H
#define TIB_SPAT_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "j2735_codec.h"
#include "TIB_config.h"
#include "traffic_signal_status_updating.h"

#define MAX_NUM_SIGNAL_GROUP 2
#define to_TimeMark(tmark) \
    (((tmark).min * 60 * 10) + ((tmark).sec * 10) + ((tmark).ms / 100))

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

time_t compare_to_tc_time;

int spat_msg_init(SPAT **pp_spat);
int spat_msg_update(SPAT *pp_spat);
void print_spat(SPAT **pp_spat);

extern SPAT *p_spat;

#endif
