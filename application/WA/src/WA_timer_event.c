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
#include "cms.h"

LOG_USE_MODULE(WA);

void WA_Agent_timer_handler(__sigval_t value)
{
    const int directionPairs[4][2] = {{0, 1}, {0, 3}, {2, 1}, {2, 3}};

    wa_vehicle_t fusion_Leading_Vehicles[4] = {
        {.speed = -1.0, .distance = -1.0},
        {.speed = -1.0, .distance = -1.0},
        {.speed = -1.0, .distance = -1.0},
        {.speed = -1.0, .distance = -1.0}   
    };

    int warningLevels[4] = {0,0,0,0}; // 0: no show, 1: Lv1, 2: lv2, 3: lv3.
    /* post-encroachment time; the expected difference time between the two confict leading vehicles reaches the intersection */
    double PET[4] = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX}; 
    /* initialize time to intersection (TTI) */
    double TTI[4] = {0.0};
    bool vehicle_in_range = false;
    WA_get_leading_vehicles(fusion_Leading_Vehicles);
    /* Test all directions */
    for (int i = 0; i < 4; i++){
        LOG_MSG_TRACE("Direction: %d, fusion_Leading_Vehicles.Speed: %lf, fusion_Leading_Vehicles.Distance: %lf",
                         i, fusion_Leading_Vehicles[i].speed, fusion_Leading_Vehicles[i].distance);
    }
    /* === decision making part of the warning algorithm === 
     * calculating TTI by distance and speed of the leading vehicles for each direction
     * then determines PET bewtween the directions pairs
     * finally assigns appropriate warning levels:
     *  - Level 1: potential conflict
     *  - Level 2: more critical conflict
     *  - Level 3: most critical conflict
     */
    for (int i = 0; i < 4; i++){
        if (fusion_Leading_Vehicles[i].distance < WA_config.warning_range) {
            if(fusion_Leading_Vehicles[i].speed > 0) {
                // calculate TTI = distance / speed; valid when car is moving
                TTI[i] = fusion_Leading_Vehicles[i].distance / fusion_Leading_Vehicles[i].speed;
                vehicle_in_range = true;
            }
        }
    }
    // Proceed only if at least one conflicting direction pair has valid TTI
    if(vehicle_in_range && (TTI[0]>0 || TTI[2]>0) && (TTI[1]>0 || TTI[3]>0) ){
        // === PET evaluation and warning level assignment ===

        int validPairs[4] = {-1, -1, -1, -1}; // Stores index of valid direction pairs
        int validCount = 0;
        int belowThreePairs[4] = {-1, -1, -1, -1}; // Stores pairs where PET ≤ 3s
        int belowThreeCount = 0;
        for (int i = 0; i < 4; i++) {
            int dir1 = directionPairs[i][0];
            int dir2 = directionPairs[i][1];
            if(TTI[dir1]>0 && TTI[dir2]>0) {
                // Compute PET: time difference between two directions reaching the intersection
                PET[i] = fabs(TTI[dir1]-TTI[dir2]);
                validPairs[validCount++] = i;

                // check if less than or equal to 3
                if(PET[i]<=3){
                    // Store direction pairs with high conflict risk (PET ≤ 3s)
                    belowThreePairs[belowThreeCount++] = i;
                }
            }  
        }
        
        if (belowThreeCount==0) {
            // === No high-risk PET pairs: assign Level 1 warning to all valid directions ===
            for(int i = 0; i < validCount; i++){
                int dir1 = directionPairs[validPairs[i]][0];
                int dir2 = directionPairs[validPairs[i]][1];
                warningLevels[dir1] = max_int(warningLevels[dir1], 1);
                warningLevels[dir2] = max_int(warningLevels[dir2], 1);
            }
        }
        else if (belowThreeCount==1) {
            // === Exactly one high-risk PET pair: apply more detailed judgment ===
            int dir1 = directionPairs[belowThreePairs[0]][0];
            int dir2 = directionPairs[belowThreePairs[0]][1];
            double dist1 = fusion_Leading_Vehicles[dir1].distance;
            double dist2 = fusion_Leading_Vehicles[dir2].distance;

            if(WA_config.collector_to_arterial == 1){
                // the distance of both leading vechicles is less than the collector to aterial warning range               
                if(dist1 <= WA_config.collector_to_arterial_warning_range && dist2 <= WA_config.collector_to_arterial_warning_range){
                    // Assign warning levels based on main direction
                    if(WA_config.maindirection == 0){// Main direction = North-South
                        if (dir1 == 0 || dir1 == 2){
                            warningLevels[dir1] = max_int(warningLevels[dir1], 2);
                            warningLevels[dir2] = max_int(warningLevels[dir2], 3);
                        } else {
                            warningLevels[dir1] = max_int(warningLevels[dir1], 3);
                            warningLevels[dir2] = max_int(warningLevels[dir2], 2);
                        }
                    }
                    else { // Main direction = East-West
                        if (dir1 == 1 || dir1 == 3) {
                            warningLevels[dir1] = max_int(warningLevels[dir1], 2);
                            warningLevels[dir2] = max_int(warningLevels[dir2], 3);
                        } else {
                            warningLevels[dir1] = max_int(warningLevels[dir1], 3);
                            warningLevels[dir2] = max_int(warningLevels[dir2], 2);
                        }
                    }
                    // All other valid pairs get Level 1 warning
                    for (int i = 0; i < validCount; i++){
                        int vdir1 = directionPairs[validPairs[i]][0];
                        int vdir2 = directionPairs[validPairs[i]][1];
                        warningLevels[vdir1] = max_int(warningLevels[vdir1], 1);
                        warningLevels[vdir2] = max_int(warningLevels[vdir2], 1);
                    }
                }
                else{ // assign based on TTI when one or both leading vehicles are out of range
                    if(TTI[dir1] <= TTI[dir2]){ 
                        warningLevels[dir1] = max_int(warningLevels[dir1], 2);
                        warningLevels[dir2] = max_int(warningLevels[dir2], 3);
                    }
                    else {
                        warningLevels[dir1] = max_int(warningLevels[dir1], 3);
                        warningLevels[dir2] = max_int(warningLevels[dir2], 2);
                    }
                    // Other valid directions get Level 1 warning
                    for (int i = 0; i < validCount; i++){
                        int vdir1 = directionPairs[validPairs[i]][0];
                        int vdir2 = directionPairs[validPairs[i]][1];
                        warningLevels[vdir1] = max_int(warningLevels[vdir1], 1);
                        warningLevels[vdir2] = max_int(warningLevels[vdir2], 1);
                    }
                }
            }
            /* collector to collector: lv2 warning message to first apporach, lv3 warning message to last approach */
            else if(WA_config.collector_to_arterial == 0){
                if(TTI[dir1] <= TTI[dir2]){ 
                    warningLevels[dir1] = max_int(warningLevels[dir1], 2);
                    warningLevels[dir2] = max_int(warningLevels[dir2], 3);
                }
                else {
                    warningLevels[dir1] = max_int(warningLevels[dir1], 3);
                    warningLevels[dir2] = max_int(warningLevels[dir2], 2);
                }
                
                // Assign Level 1 warning to other direction pairs
                for (int i = 0; i < validCount; i++){
                    int vdir1 = directionPairs[validPairs[i]][0];
                    int vdir2 = directionPairs[validPairs[i]][1];
                    warningLevels[vdir1] = max_int(warningLevels[vdir1], 1);
                    warningLevels[vdir2] = max_int(warningLevels[vdir2], 1);
                }
            }
            else {
                LOG_MSG_ERROR("Invalid collector_to_arterial value: %d", WA_config.collector_to_arterial);
            }
        }
        else {
            // === Multiple (≥2) high-risk PET pairs: assign Level 3 to all valid directions ===
            for(int i = 0; i < validCount ; i++){
                int dir1 = directionPairs[validPairs[i]][0];
                int dir2 = directionPairs[validPairs[i]][1];
                warningLevels[dir1] = max_int(warningLevels[dir1], 3);
                warningLevels[dir2] = max_int(warningLevels[dir2], 3);
            }
        }
    }
    uint8_t buf[CMS_NUM_MAX] = {0};
    bool show_flag = false;
    const uint8_t warning_map[] = {255, 215, 216, 217}; //215~224 警示訊息 Reserved，以 215 為 lv 1 警示訊息向下遞增

    /* see if any warning levels should be showed (bigger than zero)) */
    for(int i = 0; i < 4; i++){
        show_flag = show_flag || (warningLevels[i] != 0);
        buf[i] = warning_map[warningLevels[i]]; 
    }
    // send warning message to CMS
    if(show_flag){
        CMS_request_end(WA.id);
        CMS_request_start(WA.id, WA.priority, buf);
    }
    else{ // no warning message to show
        CMS_request_end(WA.id);
    }
    for (int i = 0; i < 4; i++){
        if(PET[i]!=DBL_MAX){
            LOG_MSG_INFO("Direction Pair %d to %d: PET %lf", directionPairs[i][0], directionPairs[i][1], PET[i]);
        }
    }
    for (int i = 0; i < 4; i++) {
        LOG_MSG_INFO("Direction %d: Warning Level %d", i, warningLevels[i]);
    }
    WA_clear_leading_vehicles();
}

