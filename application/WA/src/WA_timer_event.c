#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>


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
    const int directionPairs[4][2] = {
        {0, 1},
        {0, 3},
        {2, 1},
        {2, 3}
    };

    CCI fusion_Leading_Vehicles[4];
    /* initialize fusion leading vehicles */
    for(int i = 0; i < 4; i++){
        fusion_Leading_Vehicles[i].speed = -1.0;
        fusion_Leading_Vehicles[i].distance = -1.0;
    }
    /* initialize time to intersection */
    int TTI[4] = {0};
    bool vehicle_in_range = false;
    WA_get_leading_vehicles(fusion_Leading_Vehicles);
    /* Test all directions */
    for (int i = 0; i < 4; i++){
        log_file_write("Direction: %d, fusion_Leading_Vehicles.Speed: %lf, fusion_Leading_Vehicles.Distance: %lf",
                         i, fusion_Leading_Vehicles[i].speed, fusion_Leading_Vehicles[i].distance);
    }
    /* actual code */
    for (int i = 0; i < 4; i++){
        if (fusion_Leading_Vehicles[i].distance < WA_config.warning_range) {
            if(fusion_Leading_Vehicles[i].speed > 0) {
                TTI[i] = fusion_Leading_Vehicles[i].distance / fusion_Leading_Vehicles[i].speed;
                vehicle_in_range = true;
            }
        }
    }
    if(vehicle_in_range && (TTI[0]>0 || TTI[2]>0) && (TTI[1]>0 || TTI[3]>0) ){
        int TTC[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
        int validPairs[4] = {-1, -1, -1, -1};
        int validCount = 0;
        int belowThreePairs[4] = {-1, -1, -1, -1};
        int belowThreeCount = 0;
        for (int i = 0; i < 4; i++) {
            int dir1 = directionPairs[i][0];
            int dir2 = directionPairs[i][1];
            if(TTI[dir1]>0 && TTI[dir2]>0) {
                TTC[i] = abs(TTI[dir1]-TTI[dir2]);
                validPairs[validCount++] = i;

                // check if less than or equal to 3
                if(TTC[i]<=3){
                    belowThreePairs[belowThreeCount++] = i;
                }
            }  
        }
    }
    log_file_write("warning_range: %d m", WA_config.warning_range);
    WA_clear_leading_vehicles();
}

