#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "log.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

void compensation_buffer_clear()
{
    memset(compensation_buffer, 0, sizeof(compensation_buffer));
    printf("\r\ncompensation_buffer is cleared\r\n");
    printf("total compensation:%d\r\n", get_total_compensation_second());
    log_file_write("compensation_buffer is cleared\r\n");
}

uint8_t is_in_compensation()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    uint8_t current_phase = get_current_phase();

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    // 只能那個step 的起始秒數去跟plan pretime 比較
    // 如果不一樣就是在補償
    // 因為補償是下個周期去對齊起始點不一樣就補償
    // 準備要補償時，換相那瞬間的秒數會被更新到PreTimeCompensated
    // 所以當PreTimeCompensated ！= pregreen就是代表正在補償
    if (signal_status.ControlStrategy == 1) {
        if (signal_status.plan[current_phase - 1].PreTimeCompensated > 0) {
            if (signal_status.plan[current_phase - 1].PreGreen !=
                signal_status.plan[current_phase - 1].PreTimeCompensated) {
                snprintf(
                    log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "signal_status.plan[%d].PreTimeCompensated:%d\r\n",
                    current_phase - 1,
                    signal_status.plan[current_phase - 1].PreTimeCompensated);
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "signal_status.plan[%d].PreGreen:%d\r\n",
                         current_phase - 1,
                         signal_status.plan[current_phase - 1].PreGreen);
                log_file_write(log_content);
                printf("do compensation\r\n");
                log_file_write("do compensation\r\n");
                return true;  // 正在補償
            } else {
                printf("No compensation\r\n");
                log_file_write("No compensation\r\n");
                return false;
            }
        } else
            return false;
    } else {
        for (int i = 0; i < CYCLE_NUM; i++) {
            for (int j = 0; j < SUBPHASEID_NUM; j++)
                if (command_buf[i][j].app_id == COMPENSATION_ID)
                    return true;
        }
        return false;
    }
}

// 取得進行零時零分基準點補償與現在補償的差距
int get_zero_alignment_compensation_time(traffic_signal_status_t *signal_status)
{
    const uint32_t daySec = 86400;
    uint16_t cycleTime, offset, subPhaseID;
    int secInDay, compTime = 0;
    struct timeval tv;
    struct tm timeinfo;

    cycleTime = signal_status->CycleTime;
    offset = signal_status->Offset;
    subPhaseID = signal_status->SubPhaseID - 1;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    // 減掉時差 與 加上 與 tc 時間的誤差
    secInDay = (timeinfo.tm_hour * 60 + timeinfo.tm_min) * 60 + timeinfo.tm_sec - offset + signal_status->tcTimeOffest;
    // 要對齊第一個時向的第一個步階的第一秒
    // 先扣掉經過的時向
    for (int i = 0; i < subPhaseID; i++) {
        secInDay -= (signal_status->plan[i].PreTimeCompensated + signal_status->plan[i].PedGreenFlash +
                     signal_status->plan[i].PedRed + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed);
    }
    // 再扣除經過的步階
    switch (signal_status->StepID) {
    case 5:
        secInDay -= signal_status->plan[subPhaseID].Yellow;
    case 4:
        secInDay -= signal_status->plan[subPhaseID].PedRed;
    case 3:
        secInDay -= signal_status->plan[subPhaseID].PedGreenFlash;
    case 2:
        secInDay -= signal_status->plan[subPhaseID].PreTimeCompensated;
    default:
        break;
    }
    // 最後扣除清過的秒數
    if (signal_status->StepID == 1)
        secInDay -= signal_status->plan[subPhaseID].PreTimeCompensated;
    else
        signal_status->plan[subPhaseID].StepArr[signal_status->StepID - 1];
    secInDay += signal_status->StepSec;

    // 避免是凌晨 0 點扣到變成負的 (前一天)
    secInDay = (secInDay + daySec) % daySec;  // 絕對值
    compTime = secInDay % cycleTime;

    // 小於 cycleTime 的 1/2 就用扣的 大於就用加的去對齊
    return (cycleTime / 2) < compTime ? -compTime : cycleTime - compTime;
}

static void inline insert_compensation_command(traffic_signal_status_t *signal_status, int Comp_cyclenum, int cycle_index, uint16_t *subphase_compensation_time)
{
    char log_content[LOG_CONTENT_LEN + 1];
    tsc_command_t command;
    int ret = 0, subPhaseID = signal_status->SubPhaseID - 1;

    memset(log_content, 0, sizeof(log_content));
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_name, COMPENSATION_NAME, COMPENSATION_MAX_LEN);

    // 把 subphase_compensation_time 裡面的值下到 tc
    // 從下一個時向開始

    command.compensation_cycle = Comp_cyclenum;
    for (int j = signal_status->SubPhaseID % signal_status->SubPhaseCount;
         j == subPhaseID; j += (j + 1) % signal_status->SubPhaseCount) {
        if (j < signal_status->SubPhaseID)
            command.cycle = (cycle_index + 1) % CYCLE_NUM;
        else
            command.cycle = cycle_index % CYCLE_NUM;
        command.target_phase = command.phase = j + 1;
        command.compensation_time = subphase_compensation_time[j];
        command.effect_time = signal_status->plan[j].PreTimeCompensated - subphase_compensation_time[j];

        printf("cycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)\r\n",
               command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
        ret = command_buf_insert_effect_time(&command);
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),
                 "\ncycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)",
                 command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
    }
    if (signal_status->StepID != 1 &&
        signal_status->StepSec - subphase_compensation_time[subPhaseID] < signal_status->plan[subPhaseID].MinGreen) {
        cycle_index++;
    }
    command.cycle = cycle_index % CYCLE_NUM;
    command.target_phase = command.phase = signal_status->SubPhaseID;
    command.compensation_time = subphase_compensation_time[subPhaseID];
    command.effect_time = signal_status->plan[subPhaseID].PreTimeCompensated - subphase_compensation_time[subPhaseID];

    printf("cycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)\r\n",
           command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
    ret = command_buf_insert_effect_time(&command);
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),
             "\ncycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)",
             command.cycle, command.phase, command.effect_time, command.compensation_time, ret);
    log_file_write(log_content);
}

void traffic_compensation_method1(uint8_t Comp_cyclenum)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "start compensation 1\r\n");
    log_file_write(log_content);

    int16_t compensation_time = 0;
    int16_t effect_time = 0;
    int ret = 0;

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    int16_t T = get_total_compensation_second();
    uint8_t current_phase = get_current_phase();
    uint8_t current_step = get_current_step();
    uint16_t current_second = get_current_second();
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "Total compensation second:%d\r\n",
             T);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "compensation cycle is %d\r\n",
             Comp_cyclenum);
    log_file_write(log_content);

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_name, COMPENSATION_NAME, COMPENSATION_MAX_LEN);

    memset(log_content, 0, sizeof(log_content));

    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        compensation_time = compensation_buffer[i] / Comp_cyclenum;
        if (compensation_time != 0) {
            for (int j = 0; j < Comp_cyclenum; j++) {
                effect_time =
                    signal_status.plan[i].PreGreen - compensation_time;
                if (effect_time <= signal_status.plan[i].MinGreen) {
                    effect_time = signal_status.plan[i].MinGreen;
                    compensation_time = signal_status.plan[i].PreGreen -
                                        effect_time;  // 更新這次補償時間
                }
                if (effect_time >= signal_status.plan[i].MaxGreen) {
                    effect_time = signal_status.plan[i].MaxGreen;
                    compensation_time = signal_status.plan[i].PreGreen -
                                        effect_time;  // 更新這次補償時間
                }
                // insert to command buffer cycle
                if (i < (signal_status.SubPhaseID - 1))
                    command.cycle = (j + 1) % CYCLE_NUM;
                else
                    command.cycle = j;
                command.phase = i + 1;
                command.target_phase = i + 1;
                command.effect_time = effect_time;
                command.compensation_time = compensation_time;
                command.compensation_cycle = Comp_cyclenum;
                printf("cycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)\r\n",
                       command.cycle, command.phase, command.effect_time, command.compensation_time, ret);

                for (int k = 0; k < SUBPHASEID_NUM; k++) {
                    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),
                             "compensation_buffer %d : %d \r\n", k, compensation_buffer[k]);
                }
                ret = command_buf_insert_effect_time(&command);
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\ncycle: %d, phase: %d, effect time: %d "
                         ",compensation_time: %d (%d)",
                         command.cycle, command.phase, command.effect_time,
                         command.compensation_time, ret);
                compensation_buffer[i] = compensation_buffer[i] - compensation_time;
                compensation_time = compensation_buffer[i];
            }
        }
    }
    log_file_write(log_content);
}

// 依照設定權重比例分配補長時間
void traffic_compensation_method2(uint8_t Comp_cyclenum, float phase_weight[PHASE_COUNT_MAX_NUM])
{
    char log_content[LOG_CONTENT_LEN + 1];
    traffic_signal_status_t signal_status;

    int16_t total_compensation_time, tmp_comp;
    int16_t effect_time[SUBPHASEID_NUM];
    int16_t subphase_compensation_time[SUBPHASEID_NUM];
    int16_t cycle_compensations[Comp_cyclenum];  // 一個週期要補償幾秒


    int ret = 0;
    int cycle_index = 0;

    get_traffic_signal_status(&signal_status);
    total_compensation_time = get_zero_alignment_compensation_time(&signal_status);  // 總補償秒數
    tmp_comp = total_compensation_time;

    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "start compensation 2\r\n");

    // 平均的分配到每個周期 最後多的秒數加在第一個周期
    for (int i = 1; i < Comp_cyclenum; i++) {
        cycle_compensations[i] = tmp_comp / Comp_cyclenum;
        tmp_comp -= cycle_compensations[i];
    }
    cycle_compensations[0] = tmp_comp;

    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),
             "Total compensation second:%d\r\n compensation cycle is %d\r\n", total_compensation_time, Comp_cyclenum);
    for (int i = 0; i < Comp_cyclenum; i++) {
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),
                 "\ncompensation cycle %d is %d", i, cycle_compensations[i]);
    }
    log_file_write(log_content);
    // 如果補償時間為 0 不做事
    if (total_compensation_time == 0)
        return;

    // initialization
    memset(effect_time, 0, sizeof(effect_time));

    for (int i = 0; i < Comp_cyclenum; i++) {
        uint16_t remaining_time = cycle_compensations[i], preremaining = 0;
        double weights[SUBPHASEID_NUM] = {0}, overfill_weight = 0;
        int compensable = signal_status.SubPhaseCount;

        memset(subphase_compensation_time, 0, sizeof(subphase_compensation_time));
        for (int j = 0; j < signal_status.SubPhaseCount; j++)
            weights[j] = phase_weight[j] * 0.01;

        // 計算出每個時向要補多少
        // remaining_time 回補過一輪後還剩下的秒數 會依照比例繼續分給其他的時向
        while (remaining_time > 0 && preremaining != remaining_time) {
            for (int j = 0; j < signal_status.SubPhaseCount; j++) {
                if (weights[j] != 0)
                    weights[j] += overfill_weight / compensable;
            }

            cycle_compensations[i] = remaining_time;
            for (int j = 0; j < signal_status.SubPhaseCount && remaining_time > 0; j++) {
                int comp_time = ceil(cycle_compensations[i] * weights[j]);
                int effect_time = signal_status.plan[j].PreTimeCompensated - subphase_compensation_time[j] - comp_time;
                if (effect_time < signal_status.plan[j].MinGreen || effect_time > signal_status.plan[j].MaxGreen) {
                    if (effect_time < signal_status.plan[j].MinGreen)
                        effect_time = signal_status.plan[j].MinGreen;
                    else
                        effect_time = signal_status.plan[j].MaxGreen;
                    overfill_weight += weights[j];
                    weights[j] = 0;
                    compensable--;
                }
                comp_time = signal_status.plan[j].PreTimeCompensated - subphase_compensation_time[j] - effect_time;
                subphase_compensation_time[j] += comp_time;
                remaining_time -= comp_time;
            }
        }

        insert_compensation_command(&signal_status, Comp_cyclenum, i, subphase_compensation_time);
    }
}
/***************
幹支道明顯的道路
 1. 延長延幹道
 2. 縮短縮支道
***************/
void traffic_compensation_method3(uint8_t Comp_cyclenum)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "start compensation 3\r\n");
    log_file_write(log_content);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    int temp_ack_seq;
    uint8_t atrerial_phase = 0, branch_phase = 1;

    for (int i = 1; i < signal_status.SubPhaseCount; i++) {
        if (signal_status.plan[i].PreGreen >= signal_status.plan[atrerial_phase].PreGreen) {
            branch_phase = atrerial_phase;
            atrerial_phase = i;
        } else if (signal_status.plan[i].PreGreen != signal_status.plan[atrerial_phase].PreGreen) {
            if (branch_phase == atrerial_phase || signal_status.plan[i].PreGreen > signal_status.plan[branch_phase].PreGreen) {
                branch_phase = i;
            }
        }
    }

    atrerial_phase += 1;
    branch_phase += 1;

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "atrerial_phase:%d \r\n branch_phase:%d\r\n", atrerial_phase, branch_phase);
    log_file_write(log_content);

    int16_t compensation_time = 0;
    int16_t effect_time = 0;

    memset(log_content, 0, sizeof(log_content));

    int16_t T = get_total_compensation_second();
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "Total compensation second:%d\r\n",
             T);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "compensation cycle is %d\r\n",
             Comp_cyclenum);

    int16_t branch_pretime = signal_status.plan[branch_phase - 1].PreGreen;
    int16_t atrerial_pretime = signal_status.plan[atrerial_phase - 1].PreGreen;
    uint16_t branch_min_green = signal_status.plan[branch_phase - 1].MinGreen;
    uint16_t atrerial_max_green =
        signal_status.plan[atrerial_phase - 1].MaxGreen;

    int ret = 0;

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_name, COMPENSATION_NAME, COMPENSATION_LEN);

    // 補償周期數為、T為總調整秒數
    // 進行負補償
    if (T > 0) {
        printf("minus compensation\r\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "minus compensation\r\n");
        log_file_write(log_content);
        memset(log_content, 0, sizeof(log_content));

        command.target_phase = branch_phase;
        compensation_time = T / Comp_cyclenum;  // 一開始預設
        for (int i = 0; i < Comp_cyclenum; i++) {
            effect_time = branch_pretime - compensation_time;
            if (effect_time < branch_min_green) {
                effect_time = branch_min_green;
                compensation_time =
                    branch_pretime - effect_time;  // 更新補償時間
            }
            if (branch_phase < signal_status.SubPhaseID)
                command.cycle = (i + 1) % CYCLE_NUM;
            else
                command.cycle = i;
            command.phase = branch_phase;
            command.effect_time = effect_time;
            command.compensation_cycle = Comp_cyclenum;
            command.compensation_time = (-1) * compensation_time;
            printf(
                "cycle: %d, phase: %d, effect time: %d ,compensation_time: %d "
                "(%d)\r\n",
                command.cycle, command.phase, command.effect_time,
                command.compensation_time, ret);
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d "
                     ",compensation_time: %d (%d)",
                     command.cycle, command.phase, command.effect_time,
                     command.compensation_time, ret);
            T = T - compensation_time;  // 剩餘多少補償時間
            compensation_time = T;
        }
        log_file_write(log_content);
    } else if (T < 0) {  // 進行正補償
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "positive compensation\r\n");
        command.target_phase = atrerial_phase;
        T = abs(T);
        compensation_time = T / Comp_cyclenum;
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "compensation_time:%d", compensation_time);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "T: %d", T);
        log_file_write(log_content);
        memset(log_content, 0, sizeof(log_content));

        for (int i = 0; i < Comp_cyclenum; i++) {
            effect_time = atrerial_pretime + compensation_time;
            if (effect_time > atrerial_max_green) {
                effect_time = atrerial_max_green;
                compensation_time =
                    effect_time - atrerial_pretime;  // 更新補償時間
            }

            if (atrerial_phase < signal_status.SubPhaseID)
                command.cycle = (i + 1) % CYCLE_NUM;
            else
                command.cycle = i;
            command.phase = atrerial_phase;
            command.effect_time = effect_time;
            command.compensation_cycle = Comp_cyclenum;
            command.compensation_time = compensation_time;
            printf(
                "cycle: %d, phase: %d, effect time: %d ,compensation_time: %d "
                "(%d)\r\n",
                command.cycle, command.phase, command.effect_time,
                command.compensation_time, ret);
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d "
                     ",compensation_time: %d (%d)",
                     command.cycle, command.phase, command.effect_time,
                     command.compensation_time, ret);
            T = T - compensation_time;  // 剩餘多少補償時間
            compensation_time = T;
        }
        log_file_write(log_content);
    }
}
