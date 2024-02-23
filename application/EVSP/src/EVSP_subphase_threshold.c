#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timerfd.h>

#include "EVSP.h"
#include "EVSP_OBU_list.h"
#include "EVSP_subphase_threshold.h"
#include "EVSP_touching_area.h"
#include "config.h"
#include "gps_information.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

#define MIN_EXPECT_SPEED 11  // 11 m/s 40 km/hr

#define THESHOLD_BUFFER 5

EVSP_activate_OBU_t activate_OBU = {
    .host_OBU_name = {0},
    .activate_mutex = PTHREAD_MUTEX_INITIALIZER,
    .activate_thread = 0,
    .control_subphaseID = 0,
    .stop = 0,
};

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
           (host_OBU->speed > MIN_EXPECT_SPEED ? host_OBU->speed : MIN_EXPECT_SPEED);
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
    int subPhase_time = signal_status->StepSec + signal_status->plan[current_subPhase].PedGreenFlash +
                        signal_status->plan[current_subPhase].PedRed + signal_status->plan[current_subPhase].Yellow +
                        signal_status->plan[current_subPhase].AllRed;
    bool is_main_subphase = true;  // 是不是主要分相

    log_snprintf(log_content,
                 "\ncurrent_subPhase %d target_phase %d\n"
                 "expect_arrival_time %.01f subphase_threshold %.01f\n"
                 "vehicle prediction speed %.2f, MIN_EXPECT_SPEED %d\n",
                 current_subPhase, target_phase, expect_arrival_time, subphase_threshold, host_OBU->speed, MIN_EXPECT_SPEED);

    // 判斷是不是主要分相 (支道或幹道)
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        int subPhaseID = plan->plan_subPhase[i].SubPhaseID;
        printf("target_green %d signal_status->plan[subPhaseID - 1].Green %d\n", target_green, signal_status->plan[subPhaseID - 1].Green);
        if (subPhaseID != target_phase && target_green <= signal_status->plan[subPhaseID - 1].Green) {
            is_main_subphase = false;
            break;
        }
    }

    if (is_main_subphase == false) {
        // 目標分相為次要分相使用長時間平滑控制 (目標分相在支道)
        // 如果有兩個以上的綠燈時間長度一樣代表兩向的權重一樣，這時候使用短時間快速輪轉
        log_snprintf(log_content, "Long time smooth control\n");
        int BeforeMtargetSubPhase = 0;
        int accumulation_target = 0;
        // 尋找主目標分相
        for (int i = (current_subPhase - 1);
             ((i != (target_phase - 1)) || (subphase_threshold > (accumulation_target + subPhase_time))) && BeforeMtargetSubPhase < 10;) {
            printf("subphase_threshold %f accumulation_target + subPhase_time %d\n", subphase_threshold, accumulation_target + subPhase_time);
            accumulation_target += subPhase_time;
            i = (i + 1) % signal_status->SubPhaseCount;
            subPhase_time = signal_status->plan[i].Green + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed;
            BeforeMtargetSubPhase++;
        }
        log_snprintf(log_content, "main target subphase start %d, end %d\n", accumulation_target, accumulation_target + subPhase_time);

        if (BeforeMtargetSubPhase >= 10) {
            log_snprintf(log_content, "BeforeMtargetSubPhase over limit, %d\n", BeforeMtargetSubPhase);
            return -1;
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
        // 目標分相為主要分相使用短時間快速輪轉 (目標分相在幹道)
        log_snprintf(log_content, "short time quick control\ncursubphase end %d subphase_threshold %.2f\n",
                     subPhase_time, subphase_threshold);
        if (subPhase_time >= subphase_threshold) {
            // 如果現在的時相
            if (target_phase == current_subPhase) {
                adjust_time = pretime + EVSP_extend_formula(target_phase, signal_status, 20);
                log_snprintf(log_content, "extend main target subphase\n");
            } else {
                adjust_time = 0;
                log_snprintf(log_content, "change to next subphase\n");
            }
        } else {
            log_snprintf(log_content, "current subphase out of the subphase_threshold\n");
            adjust_time = pretime;
        }
    }
    log_snprintf(log_content, "adjust_time %d\n", adjust_time);
    return adjust_time;
}

void *EVSP_OBU_activation_timer()
{
    int fd = set_timer_fd(1, "EVSP_OBU_activation_timer");
    traffic_signal_status_t signal_status;
    tsc_command_t command = {0};
    EVSP_touching_area_t *area_ptr = NULL;
    EVSP_host_OBU_obj_t *host_OBU;

    char log_content[LOG_CONTENT_LEN + 1] = {0};

    if (fd == -1) {
        return NULL;
    }

    command.app_id = EVSP.id;
    command.app_priority = EVSP.priority;
    strncpy(command.host_OBU_name, activate_OBU.host_OBU_name, sizeof(activate_OBU.host_OBU_name));

    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            log_file_write_fatal_error("EVSP_OBU_activation_timer timer read error");
        memset(log_content, 0, sizeof(log_content));
        get_traffic_signal_status(&signal_status);

        EVSP_plan_table_t *plan = EVSP_plan_table_search(signal_status.PlanID);
        int ret = -1;
        int target_phase;
        if (plan == NULL) {
            log_snprintf(log_content, "EVSP_OBU_activation_timer touching area plan not found");
            log_file_write(log_content);
            break;
        }


        if (signal_status.SubPhaseID == activate_OBU.control_subphaseID) {
            log_file_write(log_content);
            continue;
        }

        pthread_mutex_lock(&activate_OBU.activate_mutex);
        host_OBU = EVSP_host_OBU_obj_search(activate_OBU.host_OBU_name);
        if (activate_OBU.stop == 0 || host_OBU == NULL) {
            pthread_mutex_unlock(&activate_OBU.activate_mutex);
            break;
        }


        target_phase = EVSP_activate(host_OBU->lon, host_OBU->lat,
                                     host_OBU->direction, plan, &area_ptr);
        printf("asds-------adasdasdasd %d-- %d---\n", target_phase, host_OBU->target_phase);
        printf("asds--asd %f-- %f--- %d\n", host_OBU->lon, host_OBU->lat, host_OBU->direction);
        if (target_phase == host_OBU->target_phase) {
            ret = EVSP_opptimiztion(target_phase, host_OBU, &signal_status, log_content);
            printf("asdsadasdasdasd %d-----\n", ret);
            if (ret != -1) {
                printf("asdsadasdasdasd\n");
                command.target_phase = target_phase;
                command.phase = signal_status.SubPhaseID;
                command.effect_time = ret;
                ret = command_buf_insert_effect_time(&command);

                log_snprintf(log_content, "\ncycle: %d, phase: %d, effect time: %d (%d)",
                             command.cycle, command.phase, command.effect_time, ret);
                if (ret != -1) {
                    activate_OBU.control_subphaseID = signal_status.SubPhaseID;
                }
            }
        }
        pthread_mutex_unlock(&activate_OBU.activate_mutex);
        if (ret != -1) {
            EVSP_host_OBU_obj_print();
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&activate_OBU.activate_mutex);
    activate_OBU.activate_thread = 0;
    activate_OBU.control_subphaseID = 0;
    memset(activate_OBU.host_OBU_name, 0, sizeof(activate_OBU.host_OBU_name));
    pthread_mutex_unlock(&activate_OBU.activate_mutex);
    log_file_write("EVSP_OBU_activation_timer close");
    close(fd);
    pthread_detach(pthread_self());
}

int EVSP_OBU_activation_timer_start(EVSP_host_OBU_obj_t *host_OBU)
{
    pthread_mutex_lock(&activate_OBU.activate_mutex);
    if (activate_OBU.activate_thread != 0) {
        pthread_mutex_unlock(&activate_OBU.activate_mutex);
        return -1;
    }
    memcpy(activate_OBU.host_OBU_name, host_OBU->OBU_name, sizeof(activate_OBU.host_OBU_name));
    activate_OBU.stop = 1;
    int ret = pthread_create(&activate_OBU.activate_thread, NULL, EVSP_OBU_activation_timer, NULL);
    pthread_mutex_unlock(&activate_OBU.activate_mutex);
    return 1;
}

void EVSP_OBU_activation_time_end()
{
    pthread_mutex_lock(&activate_OBU.activate_mutex);
    activate_OBU.stop = 0;
    pthread_mutex_unlock(&activate_OBU.activate_mutex);
}