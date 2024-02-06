#ifndef TIB_SPAT_UTILS_H
#define TIB_SPAT_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "j2735_codec.h"
#include "TIB_config.h"
#include "traffic_signal_status_updating.h"

#define SPaT_Adjust_RegionalID 254

int spat_msg_init(SPAT **pp_spat);
int spat_msg_update(SPAT *pp_spat);
void spat_printf(SPAT *pp_spat);

extern SPAT *p_spat;

#endif
