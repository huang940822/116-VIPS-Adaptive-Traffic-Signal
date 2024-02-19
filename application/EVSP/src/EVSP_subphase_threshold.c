#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "EVSP_subphase_threshold.h"
#include "EVSP_touching_area.h"
#include "config.h"
#include "gps_information.h"
#include "log.h"
#include "typedefine.h"

#define EXPECT_SPEED 17  // 17m/s 60 km/hr

#define THESHOLD_BUFFER 5

int EVSP_extend_formula(int target_phase, traffic_signal_status_t *signal_status, int Tbf)
{
    int EVSP_adjust_time = 0;
    // Ｇmx＝ΣＰnb＋Ｔbf
    // Ｇmx：延長最長綠燈秒數
    // ΣＰnb：非公車各分相最短綠及清道時間總和
    // Ｔbf：緩衝誤差值，約為１０-２０秒，視觸動之距離而調整
    // 路口號誌為三時相，週期Ｃ為１２０秒，第一時相為公車方向６４秒綠燈、４秒黃燈、２秒全紅；
    // 第二時相１０秒綠燈、３秒黃燈、２秒全紅；第三時相２９秒綠燈、３秒黃燈、３秒全紅；而第二時相最短綠為５秒、第三時相最短綠為１５秒
    // Ｇmx＝【（５＋３＋２）＋（１５＋３＋３）】

    for (int i = 1; i < 8; i++) {
        if (i != target_phase)
            EVSP_adjust_time += signal_status->plan[i - 1].MinGreen +
                                signal_status->plan[i - 1].Yellow +
                                signal_status->plan[i - 1].AllRed;
    }
    return EVSP_adjust_time += Tbf;  // Gmx += 20 ，緩衝誤差值調最大
}

float get_expect_arrival_time(EVSP_host_OBU_obj_t *host_OBU)
{
    return get_distance(config.RSU_lat, config.RSU_lon, host_OBU->lat, host_OBU->lon) /
           EXPECT_SPEED;
}

float get_subphase_threshold(int target_phase, float expect_arrival_time, traffic_signal_status_t *signal_status)
{
    return expect_arrival_time - EVSP_extend_formula(target_phase, signal_status, THESHOLD_BUFFER);
}

/*

*/
int EVSP_opptimiztion(int target_phase, EVSP_host_OBU_obj_t *host_OBU, traffic_signal_status_t *signal_status, char *log_content)
{
    int adjust_time = -1;
    int current_subPhase = signal_status->SubPhaseID;
    int current_step = signal_status->StepID;

    // 只有在步階一才能調整
    if (current_step != 1)
        return adjust_time;

    float expect_arrival_time = get_expect_arrival_time(host_OBU);
    float subphase_threshold = get_subphase_threshold(target_phase, expect_arrival_time, signal_status);

    EVSP_plan_table_t *plan = EVSP_plan_table_search(signal_status->PlanID);
    uint16_t pretime = signal_status->plan[current_subPhase - 1].PreTimeCompensated;
    int tmp = 0;
    int target_green = signal_status->plan[target_phase - 1].Green;
    bool is_main_subphase = true;  // 是不是主要分相

    log_snprintf(log_content, "\nexpect_arrival_time %.01f subphase_threshold %.01f\n", expect_arrival_time, subphase_threshold);

    for (int i = 0; i < plan->plan_subPhase_count; i++) {
        int subPhaseID = plan->plan_subPhase[i].SubPhaseID;
        if (subPhaseID != target_phase && target_green <= signal_status->plan[subPhaseID - 1].Green) {
            is_main_subphase = false;
            break;
        }
    }
    if (is_main_subphase == false) {
        // 目標分相為次要分相使用長時間平滑控制
        // 如果有兩個以上的綠燈時間長度一樣代表兩向的權重一樣，這時候使用短時間快速輪轉
        log_snprintf(log_content, "Long time smooth control\n");
        int subPhase_time = signal_status->StepSec + signal_status->plan[current_subPhase].PedGreenFlash +
                            signal_status->plan[current_subPhase].PedRed + signal_status->plan[current_subPhase].Yellow +
                            signal_status->plan[current_subPhase].AllRed;
        int BeforeMtargetSubPhase = 0;
        int accumulation_target = 0;
        // 尋找主目標分相
        for (int i = current_subPhase; i == target_phase && subphase_threshold < accumulation_target + subPhase_time;) {
            printf("asd\n");
            accumulation_target += subPhase_time;
            i = (current_subPhase + 1) % signal_status->SubPhaseCount;
            subPhase_time = signal_status->plan[i].Green + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed;
            BeforeMtargetSubPhase++;
        }
        if (BeforeMtargetSubPhase == 0) {  // 在主目標分相
            adjust_time = pretime + EVSP_extend_formula(target_phase, signal_status, 20);
            log_snprintf(log_content, "extend main target subphase, extend %d\n", adjust_time);
        } else {
            log_snprintf(log_content, "BeforeMtargetSubPhase %d\n", BeforeMtargetSubPhase);
            if (expect_arrival_time < accumulation_target) {
                // 預期抵達時間比主目標分相開始早
                // 所以進行縮短
                tmp = ceil(((float) accumulation_target - expect_arrival_time) / BeforeMtargetSubPhase);
                adjust_time = pretime - tmp;
                log_snprintf(log_content, "arrive time is too early, shorten %d\n", tmp);
            } else if (accumulation_target + subPhase_time < expect_arrival_time) {
                // 預期抵達時間比主目標分相結束晚
                // 所以進行延長
                tmp = ceil((expect_arrival_time - (accumulation_target + subPhase_time)) / BeforeMtargetSubPhase);
                adjust_time = pretime + tmp;
                log_snprintf(log_content, "arrive time is too late, extend %d\n", tmp);
            } else {
                // 預期抵達時間在主目標分相時段內就甚麼都不做
                log_snprintf(log_content, "arrive time is in main target subphase\n");
                adjust_time = pretime;
            }
        }
    } else {
        // 目標分相為主要分相使用短時間快速輪轉
        log_snprintf(log_content, "short time quick control\n");
        if (signal_status->StepSec >= subphase_threshold) {
            // 如果現在的時相
            if (target_phase == current_subPhase) {
                adjust_time = pretime + EVSP_extend_formula(target_phase, signal_status, 20);
                log_snprintf(log_content, "extend main target subphase\n");
            } else {
                adjust_time = 0;
                log_snprintf(log_content, "change to next subphase\n");
            }
        }
    }
    return adjust_time;
}