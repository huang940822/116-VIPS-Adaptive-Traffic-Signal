#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    }
}

void traffic_compensation_method1()
{
    printf("start compensation 1\r\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    int16_t compensation_time = 0;
    int16_t effect_time = 0;
    int ret = 0;

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_id, COMPENSATION_NAME, COMPENSATION_LEN);

    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        compensation_time = compensation_buffer[i] / COMPENSATION_CYCLE;
        if (compensation_time != 0) {
            for (int j = 0; j < COMPENSATION_CYCLE; j++) {
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
                command.cycle = j;
                command.phase = i + 1;
                command.target_phase = i + 1;
                command.effect_time = effect_time;
                command.compensation_time = compensation_time;
                ret = command_buf_insert_effect_time(&command);
                printf(
                    "cycle: %d, phase: %d, effect time: %d ,compensation_time: "
                    "%d (%d)\r\n",
                    command.cycle, command.phase, command.effect_time,
                    command.compensation_time, ret);
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\ncycle: %d, phase: %d, effect time: %d "
                         ",compensation_time: %d (%d)",
                         command.cycle, command.phase, command.effect_time,
                         command.compensation_time, ret);
                log_file_write(log_content);
                compensation_buffer[i] =
                    compensation_buffer[i] - compensation_time;
                compensation_time = compensation_buffer[i];
            }
        }
    }
    compensation_buffer_clear();
}

void traffic_compensation_method2()
{
    printf("start compensation 2\r\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    int16_t T = get_total_compensation_second();
    int16_t effect_time[SUBPHASEID_NUM];
    int16_t compensation_time[SUBPHASEID_NUM];
    bool flag[SUBPHASEID_NUM];  // 是否要重新計算
    int16_t t[COMPENSATION_CYCLE] = {T / 2, T - T / 2};

    printf("T:%d\r\n", T);
    printf("%d %d\r\n", t[0], t[1]);
    // initialization
    memset(effect_time, 0, sizeof(effect_time));
    memset(compensation_time, 0, sizeof(compensation_time));
    memset(flag, false, sizeof(flag));

    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_id, COMPENSATION_NAME, COMPENSATION_LEN);

    int ret = 0;

    for (int i = 0; i < COMPENSATION_CYCLE; i++) {
        for (int j = 0; j < signal_status.SubPhaseCount; j++) {
            if (config.phase_weight[j] != 0) {
                if (flag[j] == false) {
                    compensation_time[j] = t[i] * config.phase_weight[j] * 0.01;
                } else {
                    int tmp = config.phase_weight[j];
                    for (int k = j + 1; j < signal_status.SubPhaseCount; k++)
                        tmp += config.phase_weight[k];
                    compensation_time[j] = t[i] * config.phase_weight[j] / tmp;
                }

                effect_time[j] =
                    signal_status.plan[j].PreGreen - compensation_time[j];
                if (effect_time[j] <= signal_status.plan[j].MinGreen) {
                    effect_time[j] = signal_status.plan[j].MinGreen;
                    compensation_time[j] =
                        signal_status.plan[j].PreGreen - effect_time[j];
                    t[i] -= compensation_time[j];
                    for (int k = j + 1; k < signal_status.SubPhaseCount; k++) {
                        flag[k] = true;
                    }
                }
                if (effect_time[j] >= signal_status.plan[j].MaxGreen) {
                    effect_time[j] = signal_status.plan[i].MaxGreen;
                    compensation_time[j] =
                        signal_status.plan[j].PreGreen - effect_time[j];
                    t[i] -= compensation_time[j];
                    for (int k = j + 1; k < signal_status.SubPhaseCount; k++) {
                        flag[k] = true;
                    }
                }
                // insert into command buffer
                command.cycle = i;
                command.phase = j + 1;
                command.target_phase = j + 1;
                command.compensation_time = compensation_time[j];
                command.effect_time = effect_time[j];
                ret = command_buf_insert_effect_time(&command);
                printf(
                    "cycle: %d, phase: %d, effect time: %d ,compensation_time: "
                    "%d (%d)\r\n",
                    command.cycle, command.phase, command.effect_time,
                    command.compensation_time, ret);
                snprintf(log_content + strlen(log_content),
                         LOG_CONTENT_LEN - strlen(log_content),
                         "\ncycle: %d, phase: %d, effect time: %d "
                         ",compensation_time: %d (%d)",
                         command.cycle, command.phase, command.effect_time,
                         command.compensation_time, ret);
                log_file_write(log_content);
            }
        }
    }
    compensation_buffer_clear();
}

// 幹支道明顯的道路
// 1. 延長延幹道
// 2. 縮短縮支道
void traffic_compensation_method3()
{
    printf("start compensation 3\r\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    int temp_ack_seq;
    uint16_t atrerial = signal_status.plan[0].PreGreen, branch = INT16_MIN;
    uint8_t atrerial_phase = 0, branch_phase = 0;
    uint8_t current_SubPhaseID = signal_status.SubPhaseID;
    uint8_t current_step = signal_status.StepID;

    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        if (signal_status.plan[i].PreGreen >= atrerial) {
            branch = atrerial;
            atrerial = signal_status.plan[i].PreGreen;
            atrerial_phase = i + 1;
        } else {
            if (signal_status.plan[i].PreGreen >= branch) {
                branch = signal_status.plan[i].PreGreen;
                branch = i + 1;
            }
        }
    }
    if (branch_phase == 0) {
        branch_phase = atrerial_phase - 1;
    }

    int16_t compensation_time = 0;
    int16_t effect_time = 0;

    int16_t T = get_total_compensation_second();
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
    strncpy(command.host_OBU_id, COMPENSATION_NAME, COMPENSATION_LEN);

    // 補償周期數為、T為總調整秒數
    // 進行負補償
    if (T > 0) {
        printf("minus compensation\r\n");
        command.target_phase = branch_phase;
        compensation_time = T / COMPENSATION_CYCLE;  // 一開始預設
        for (int i = 0; i < COMPENSATION_CYCLE; i++) {
            effect_time = branch_pretime - compensation_time;
            if (effect_time < branch_min_green) {
                effect_time = branch_min_green;
                compensation_time =
                    branch_pretime - effect_time;  //更新補償時間
            }
            command.cycle = i;
            command.phase = branch_phase;
            command.effect_time = effect_time;
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
            log_file_write(log_content);
            T = T - compensation_time;  //剩餘多少補償時間
            compensation_time = T;
        }
        compensation_buffer_clear();

    } else {  // 進行正補償
        printf("positive compensation\r\n");
        command.target_phase = atrerial_phase;
        T = abs(T);
        compensation_time = T / COMPENSATION_CYCLE;
        for (int i = 0; i < COMPENSATION_CYCLE; i++) {
            effect_time = atrerial_pretime + compensation_time;
            if (effect_time > atrerial_max_green) {
                effect_time = atrerial_max_green;
                compensation_time =
                    effect_time - atrerial_pretime;  //更新補償時間
            }

            command.cycle = i;
            command.phase = atrerial_phase;
            command.effect_time = effect_time;
            command.compensation_time = compensation_time;
            ret = command_buf_insert_effect_time(&command);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ncycle: %d, phase: %d, effect time: %d "
                     ",compensation_time: %d (%d)",
                     command.cycle, command.phase, command.effect_time,
                     command.compensation_time, ret);
            log_file_write(log_content);
            printf(
                "cycle: %d, phase: %d, effect time: %d ,compensation_time: %d "
                "(%d)\r\n",
                command.cycle, command.phase, command.effect_time,
                command.compensation_time, ret);
            T = T - compensation_time;  //剩餘多少補償時間
            compensation_time = T;
        }
        compensation_buffer_clear();
    }
}
