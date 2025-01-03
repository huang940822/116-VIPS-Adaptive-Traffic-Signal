#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "WA.h"
#include "WA_config.h"
#include "WA_timer_event.h"
#include "WA_util.h"

#include "typedefine.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "log.h"

void WA_Agent_timer_handler(__sigval_t value)
{
    CCI fusion_Leading_Vehicles[4];
    /* initialize fusion leading vehicles */
    for(int i = 0; i < 4; i++){
        fusion_Leading_Vehicles[i].speed = -1.0;
        fusion_Leading_Vehicles[i].distance = -1.0;
    }
    WA_get_leading_vehicles(fusion_Leading_Vehicles);
    for (int i = 0; i < 4; i++){
        log_file_write("Direction: %d, fusion_Leading_Vehicles.Speed: %lf, fusion_Leading_Vehicles.Distance: %lf",
                         i, fusion_Leading_Vehicles[i].speed, fusion_Leading_Vehicles[i].distance);
    }
    WA_clear_leading_vehicles();
}

