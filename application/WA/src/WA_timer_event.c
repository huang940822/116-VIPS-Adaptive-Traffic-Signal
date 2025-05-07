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

void WA_Agent_timer_handler(__sigval_t value)
{
    const int directionPairs[4][2] = {
        {0, 1},
        {0, 3},
        {2, 1},
        {2, 3}
    };

    CCI fusion_Leading_Vehicles[4];
    int warningLevels[4] = {0,0,0,0}; // 0: no show, 1: Lv1, 2: lv2, 3: lv3.
    double PET[4] = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX};
    /* initialize fusion leading vehicles */
    for(int i = 0; i < 4; i++){
        fusion_Leading_Vehicles[i].speed = -1.0;
        fusion_Leading_Vehicles[i].distance = -1.0;
    }
    /* initialize time to intersection */
    double TTI[4] = {0.0};
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
        //double PET[4] = {DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX};
        int validPairs[4] = {-1, -1, -1, -1};
        int validCount = 0;
        int belowThreePairs[4] = {-1, -1, -1, -1};
        int belowThreeCount = 0;
        for (int i = 0; i < 4; i++) {
            int dir1 = directionPairs[i][0];
            int dir2 = directionPairs[i][1];
            if(TTI[dir1]>0 && TTI[dir2]>0) {
                PET[i] = fabs(TTI[dir1]-TTI[dir2]);

                // check if less than or equal to 3
                if(PET[i]<=3){
                    belowThreePairs[belowThreeCount++] = i;
                }
                else{
                    validPairs[validCount++] = i;
                }
            }  
        }
        
        if (belowThreeCount==0) {
            /* Send Lv1 warning Message for incoming directions */
            for(int i = 0; i < validCount; i++){
                int dir1 = directionPairs[validPairs[i]][0];
                int dir2 = directionPairs[validPairs[i]][1];
                warningLevels[dir1] = 1;
                warningLevels[dir2] = 1;
            }
        }
        else if (belowThreeCount==1) {
            int dir1 = directionPairs[belowThreePairs[0]][0];
            int dir2 = directionPairs[belowThreePairs[0]][1];
            double dist1 = fusion_Leading_Vehicles[dir1].distance;
            double dist2 = fusion_Leading_Vehicles[dir2].distance;

            if(WA_config.branch2main == 1){
                /* if both car in WA_config.branch2mainrange */
                if(dist1 <= WA_config.branch2mainRange && dist2 <= WA_config.branch2mainRange){
                    if(WA_config.maindirection == 0){// N-S N:0, S:2 
                        if (dir1 == 0 || dir1 == 2){
                            warningLevels[dir1] = 2;
                            warningLevels[dir2] = 3;
                        } else {
                            warningLevels[dir1] = 3;
                            warningLevels[dir2] = 2;
                        }
                    }
                    else {
                        if (dir1 == 1 || dir1 == 3) {
                            warningLevels[dir1] = 2;
                            warningLevels[dir2] = 3;
                        } else {
                            warningLevels[dir1] = 3;
                            warningLevels[dir2] = 2;
                        }
                    }
                    for (int i = 0; i < validCount; i++){
                        int vdir1 = directionPairs[validPairs[i]][0];
                        int vdir2 = directionPairs[validPairs[i]][1];
                        warningLevels[vdir1] = 1;
                        warningLevels[vdir2] = 1;
                    }
                }
                else{
                    if(TTI[dir1] <= TTI[dir2]){ 
                        warningLevels[dir1] = 2;
                        warningLevels[dir2] = 3;
                    }
                    else {
                        warningLevels[dir1] = 3;
                        warningLevels[dir2] = 2;
                    }
                    for (int i = 0; i < validCount; i++){
                        int vdir1 = directionPairs[validPairs[i]][0];
                        int vdir2 = directionPairs[validPairs[i]][1];
                        warningLevels[vdir1] = 1;
                        warningLevels[vdir2] = 1;
                    }
                }
            }
            /* Branch2Branch: lv2 warning message to first apporach, lv3 warning message to last approach */
            else{
                if(TTI[dir1] <= TTI[dir2]){ 
                    warningLevels[dir1] = 2;
                    warningLevels[dir2] = 3;
                }
                else {
                    warningLevels[dir1] = 3;
                    warningLevels[dir2] = 2;
                }
                for (int i = 0; i < validCount; i++){
                    int vdir1 = directionPairs[validPairs[i]][0];
                    int vdir2 = directionPairs[validPairs[i]][1];
                    warningLevels[vdir1] = 1;
                    warningLevels[vdir2] = 1;
                }
            }
        }
        else {
            /* Send Lv3 warning Message for incoming directions */
            for(int i = 0; i < validCount ; i++){
                int dir1 = directionPairs[validPairs[i]][0];
                int dir2 = directionPairs[validPairs[i]][1];
                warningLevels[dir1] = 3;
                warningLevels[dir2] = 3;
            }
        }
    }
    uint8_t buf[CMS_NUM_MAX] = {0};
    bool show_flag = false;
    const uint8_t warning_map[] = {255, 215, 216, 217};
    //215~224 警示訊息 Reserved，以 215 為 lv 1 警示訊息向下遞增
    for(int i = 0; i < 4; i++){
        show_flag = show_flag || (warningLevels[i] != 0);
        buf[i] = warning_map[warningLevels[i]]; 
    }
    if(show_flag){
        CMS_request_end(WA.id);
        CMS_request_start(WA.id, WA.priority, buf);
    }
    else{
        CMS_request_end(WA.id);
    }
    for (int i = 0; i < 4; i++){
        if(PET[i]!=DBL_MAX){
            log_file_write("Direction Pair %d to %d: PET %lf", directionPairs[i][0], directionPairs[i][1], PET[i]);
        }
    }
    for (int i = 0; i < 4; i++) {
        log_file_write("Direction %d: Warning Level %d", i, warningLevels[i]);
    }
    log_file_write("warning_range: %d m", WA_config.warning_range);
    WA_clear_leading_vehicles();
}

