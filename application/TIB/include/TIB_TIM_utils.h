#ifndef TIB_TIM_UTILS_H
#define TIB_TIM_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "j2735_tim.h"
#include "TIB_config.h"
#include "traffic_signal_status_updating.h"

#define TIM_Adjust_RegionalID 249

int tim_msg_init(TravelerInformation **pp_tim);
int tim_msg_update(TravelerInformation *pp_tim);
void tim_printf(TravelerInformation *pp_tim);

extern TravelerInformation *p_tim;

#endif
