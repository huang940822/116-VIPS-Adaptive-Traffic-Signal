#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "common_packet_tx.h"
#include "config.h"
#include "log.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

LOG_USE_MODULE(MIDDLEWARE_COMPENSATION);

#define ArgTrafficStatus traffic_signal_status_t *signal_status
#define ArgLogContent char log_content[LOG_CONTENT_LEN + 1]
#define ArgLogAndStatus ArgTrafficStatus, ArgLogContent
#define PassLogAndStaus &signal_status, log_content

pthread_mutex_t mutex_compensation = PTHREAD_MUTEX_INITIALIZER;
uint8_t compensation_buffer[SUBPHASEID_NUM] = {0};  // 儲存是否有被調整過的 phase

static inline void get_compensation_buffer(uint8_t buf[SUBPHASEID_NUM])
{
    pthread_mutex_lock(&mutex_compensation);
    memcpy(buf, compensation_buffer, sizeof(compensation_buffer));
    pthread_mutex_unlock(&mutex_compensation);
}

void set_compensation_buffer(int subphaseId)
{
    pthread_mutex_lock(&mutex_compensation);
    compensation_buffer[subphaseId - 1] = 1;
    pthread_mutex_unlock(&mutex_compensation);
}

void compensation_buffer_clear()
{
    pthread_mutex_lock(&mutex_compensation);
    memset(compensation_buffer, 0, sizeof(compensation_buffer));
    pthread_mutex_unlock(&mutex_compensation);
    LOG_MSG_INFO("compensation_buffer is cleared");
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
                LOG_MSG_APPEND(log_content,
                             "signal_status.plan[%d].PreTimeCompensated:%d\n"
                             "signal_status.plan[%d].PreGreen:%d\n"
                             "do compensation",
                             current_phase - 1, signal_status.plan[current_phase - 1].PreTimeCompensated,
                             current_phase - 1, signal_status.plan[current_phase - 1].PreGreen);
                LOG_MSG_INFO(log_content);
                return true;  // 正在補償
            } else {
                LOG_MSG_INFO("No compensation");
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

// 取得基準點補償與現在補償的差距
static inline int16_t get_alignment_compensation_time(ArgTrafficStatus, int alignHour, int alignMin)
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

    // 減掉時差 跟 加上 與 tc 時間的誤差
    secInDay = ((timeinfo.tm_hour - alignHour) * 60 + (timeinfo.tm_min - alignMin)) * 60 +
               timeinfo.tm_sec - offset + signal_status->tcTimeOffest;
    // 要對齊下一個週期開始的時間
    // 加上剩下時向的時間
    for (int i = subPhaseID + 1; i < signal_status->SubPhaseCount; i++) {
        secInDay += (signal_status->plan[i].PreGreen + signal_status->plan[i].PedGreenFlash +
                     signal_status->plan[i].PedRed + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed);
    }
    // 加上剩下步階的時間
    for (int i = signal_status->StepID - 1; i < 4; i++) {
        secInDay += signal_status->plan[subPhaseID].StepArr[i];
    }
    // 加上現在剩餘的秒數
    secInDay += signal_status->StepSec;
    compTime = secInDay % cycleTime;
    LOG_MSG_INFO("Get alignment compensation time secInDay: %d align: %d %d compTime: %d cycletime: %d",
                   secInDay, alignHour, alignMin, compTime, cycleTime);
    // 小於 cycleTime 的 1/2 就用負補償 大於就用正補償
    return compTime < (cycleTime / 2) ? -compTime : cycleTime - compTime;
}

static inline void get_nearly_segment(ArgTrafficStatus, AllDay_plan_t *segment)
{
    struct timeval tv;
    struct tm timeinfo;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    int MinInDay = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    for (int i = 1; i < signal_status->SegmentCount; i++) {
        if (MinInDay < (signal_status->allday_plan[i].Hour * 60 + signal_status->allday_plan[i].Min)) {
            segment->Hour = signal_status->allday_plan[i - 1].Hour;
            segment->Min = signal_status->allday_plan[i - 1].Min;
            return;
        }
    }
    segment->Hour = signal_status->allday_plan[signal_status->SegmentCount - 1].Hour;
    segment->Min = signal_status->allday_plan[signal_status->SegmentCount - 1].Min;
}


// 在沒有額外設定下 成龍為時段基準點 山佇為零時零分基準點
// 如果不是按照這個規則他會
static inline int16_t get_total_compensation_second_with_status(ArgTrafficStatus)
{
    AllDay_plan_t segment;
    if (config.traffic_compensation_baseline == ZERO_HOUR_ZERO_MIN_BASELINE) {
        return get_alignment_compensation_time(signal_status, 0, 0);
    } else if (config.traffic_compensation_baseline == DAILY_SEGMENT_BASELINE) {
        get_nearly_segment(signal_status, &segment);
        return get_alignment_compensation_time(signal_status, segment.Hour, segment.Min);
    }

    if (config.signal_controller_manufacturer == CHENG_LONG) {
        get_nearly_segment(signal_status, &segment);
        return get_alignment_compensation_time(signal_status, segment.Hour, segment.Min);
    } else {
        return get_alignment_compensation_time(signal_status, 0, 0);
    }
}

int16_t get_total_compensation_second()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    return get_total_compensation_second_with_status(&signal_status);
}

static inline void insert_compensation_command(ArgLogAndStatus, int Comp_cyclenum, int cycle_index, int16_t subphase_compensation_time[SUBPHASEID_NUM])
{
    tsc_command_t command = {0};
    int ret = 0, subPhaseID = signal_status->SubPhaseID - 1;
    int subphase_ptr = subPhaseID;

    command.app_id = COMPENSATION_ID;
    command.app_priority = COMPENSATION_priority;
    strncpy(command.host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME));

    // 如果現在不是步階 1 或是步階 1 會調整到小於 MinGreen 或超過 MaxGreen 則到下一個 cycle
    if (signal_status->StepID != 1 ||
        signal_status->StepSec - subphase_compensation_time[subPhaseID] < signal_status->plan[subPhaseID].MinGreen ||
        signal_status->StepSec - subphase_compensation_time[subPhaseID] > signal_status->plan[subPhaseID].MaxGreen) {
        subphase_ptr = (subphase_ptr + 1) % signal_status->SubPhaseCount;
    }

    int cycle_ptr = cycle_index % CYCLE_NUM;
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        if (cycle_index >= Comp_cyclenum - 1 && subphase_compensation_time[subphase_ptr] == 0) {
            // 因為成龍會自己補償所以如果只後還有補償的話就算是 0 也會補
            int tmp_ptr = subphase_ptr;
            for (int j = i + 1; j < signal_status->SubPhaseCount; j++) {
                tmp_ptr = (tmp_ptr + 1) % signal_status->SubPhaseCount;
                if (subphase_compensation_time[tmp_ptr] != 0)
                    goto InsertCommand;
            }
            goto NotInsertCommand;
        }
    InsertCommand:
        command.cycle = cycle_ptr;
        command.target_phase = command.phase = subphase_ptr + 1;
        command.effect_time = (int16_t) signal_status->plan[subphase_ptr].PreGreen + subphase_compensation_time[subphase_ptr];

        ret = command_buf_insert_effect_time(&command);
    NotInsertCommand:
        LOG_MSG_APPEND(log_content, "cycle: %d, phase: %d, effect time: %d ,compensation_time: %d (%d)\n",
                     command.cycle, subphase_ptr + 1, signal_status->plan[subphase_ptr].PreGreen + subphase_compensation_time[subphase_ptr],
                     subphase_compensation_time[subphase_ptr], ret);
        subphase_ptr++;
        if (subphase_ptr >= signal_status->SubPhaseCount) {
            subphase_ptr %= signal_status->SubPhaseCount;
            cycle_ptr = (cycle_ptr + 1) % CYCLE_NUM;
        }
    }
}

static inline int allocate_compensation_by_weight(ArgTrafficStatus, float phase_weight[PHASE_COUNT_MAX_NUM], int compensation_time, int16_t subphase_compensation_time[SUBPHASEID_NUM])
{
    int remaining_time = compensation_time, comp_time = 0;
    float weights[SUBPHASEID_NUM] = {0};

    for (int i = 0; i < signal_status->SubPhaseCount; i++)
        weights[i] = phase_weight[i] * 0.01;

    for (int i = 0; i < signal_status->SubPhaseCount && remaining_time != 0; i++) {
        if (compensation_time > 0) {
            comp_time = ceil(compensation_time * weights[i]);                     // 取無條件進位
            comp_time = comp_time > remaining_time ? remaining_time : comp_time;  // 少於剩餘時間就等於剩餘時間
        } else {
            comp_time = -ceil(-compensation_time * weights[i]);
            comp_time = comp_time < remaining_time ? remaining_time : comp_time;
        }

        int effect_time = signal_status->plan[i].PreGreen + subphase_compensation_time[i] + comp_time;
        if (effect_time < signal_status->plan[i].MinGreen)
            effect_time = signal_status->plan[i].MinGreen;
        else if (effect_time > signal_status->plan[i].MaxGreen)
            effect_time = signal_status->plan[i].MaxGreen;

        comp_time = effect_time - signal_status->plan[i].PreGreen - subphase_compensation_time[i];
        subphase_compensation_time[i] += comp_time;
        remaining_time -= comp_time;
    }
    return remaining_time;
}

// 分配剩餘的補償時間從主時相開始補償 及綠燈秒數最長的
static inline void allocate_remaining_time(ArgTrafficStatus, int remaining_time, int16_t subphase_compensation_time[SUBPHASEID_NUM])
{
    if (remaining_time == 0)
        return;
    int pre_remaining_time = 0;
    int index[PHASE_COUNT_MAX_NUM];
    for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
        index[i] = i;
    }

    // 使用冒泡泡排序排序大小的 index
    for (int i = 0; i < signal_status->SubPhaseCount - 1; i++) {
        for (int j = 0; j < signal_status->SubPhaseCount - i - 1; j++) {
            if (signal_status->plan[index[j]].PreGreen < signal_status->plan[index[j + 1]].PreGreen) {
                int temp = index[j];
                index[j] = index[j + 1];
                index[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < signal_status->SubPhaseCount && remaining_time != 0; i++) {
        float weight[PHASE_COUNT_MAX_NUM] = {0};
        weight[index[i]] = 100;
        remaining_time = allocate_compensation_by_weight(signal_status, weight, remaining_time, subphase_compensation_time);
    }
}

// 把總補償秒數平均分給各個週期
static inline int get_cycle_compensation_time(ArgLogAndStatus, int16_t cycle_compensations[CYCLE_NUM], uint8_t Comp_cyclenum, uint8_t methodId)
{
    int16_t total_compensation_time, tmp_comp;

    total_compensation_time = get_total_compensation_second_with_status(signal_status);  // 總補償秒數
    tmp_comp = total_compensation_time;

    // 平均的分配到每個周期 最後多的秒數加在第一個周期
    for (int i = 1; i < Comp_cyclenum; i++) {
        cycle_compensations[i] = tmp_comp / Comp_cyclenum;
        tmp_comp -= cycle_compensations[i];
    }
    cycle_compensations[0] = tmp_comp;

    LOG_MSG_APPEND(log_content, "start compensation method %d \nTotal compensation second:%d\n compensation cycle is %d\n",
                 methodId, total_compensation_time, Comp_cyclenum);
    for (int i = 0; i < Comp_cyclenum; i++) {
        LOG_MSG_APPEND(log_content, "compensation cycle %d is %d\n", i, cycle_compensations[i]);
    }
    // 如果補償時間為 0 不做事
    return total_compensation_time;
}

static inline int implement_compensation_by_weight(ArgLogAndStatus, int16_t cycle_compensations[CYCLE_NUM], uint8_t Comp_cyclenum, float phase_weight[PHASE_COUNT_MAX_NUM])
{
    int16_t subphase_compensation_time[SUBPHASEID_NUM];

    for (int i = 0; i < Comp_cyclenum; i++) {
        memset(subphase_compensation_time, 0, sizeof(subphase_compensation_time));
        int16_t remaining_time =
            allocate_compensation_by_weight(signal_status, phase_weight, cycle_compensations[i], subphase_compensation_time);
        allocate_remaining_time(signal_status, remaining_time, subphase_compensation_time);
        insert_compensation_command(signal_status, log_content, Comp_cyclenum, i, subphase_compensation_time);
    }
}

// 原時向補償
static inline void traffic_compensation_method1(ArgLogContent, uint8_t Comp_cyclenum)
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    traffic_signal_status_t signal_status;

    float phase_weight[PHASE_COUNT_MAX_NUM] = {0};
    uint8_t comp_buf[SUBPHASEID_NUM] = {0};
    int adjustNum = 0;

    get_traffic_signal_status(&signal_status);
    get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 1);

    // 查詢有哪些時向是被調整過的 並分配比例
    get_compensation_buffer(comp_buf);
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        if (comp_buf[i] != 0)
            adjustNum++;
    }
    for (int i = 0; i < SUBPHASEID_NUM; i++) {
        if (comp_buf[i] != 0)
            phase_weight[i] = 100.0 / adjustNum;
        LOG_MSG_TRACE("phase_weight %d %f", i, phase_weight[i]);
    }

    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

// 依照設定權重比例分配補長時間
static inline void traffic_compensation_method2(ArgLogContent, uint8_t Comp_cyclenum, float phase_weight[PHASE_COUNT_MAX_NUM])
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    traffic_signal_status_t signal_status;

    get_traffic_signal_status(&signal_status);
    get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 2);
    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

/***************
幹支道明顯的道路
 1. 延長延幹道
 2. 縮短縮支道
***************/
static inline void traffic_compensation_method3(ArgLogContent, uint8_t Comp_cyclenum)
{
    int16_t cycle_compensations[CYCLE_NUM] = {0};  // 一個週期要補償幾秒
    int16_t total_compensation_time = 0;
    traffic_signal_status_t signal_status;

    uint8_t arterial_phase = 0, branch_phase = 255;
    float phase_weight[PHASE_COUNT_MAX_NUM] = {0};

    get_traffic_signal_status(&signal_status);
    total_compensation_time = get_cycle_compensation_time(PassLogAndStaus, cycle_compensations, Comp_cyclenum, 3);

    // 綠燈長度最長的為主幹道 第二長的為支道
    for (int i = 1; i < signal_status.SubPhaseCount; i++) {
        if (signal_status.plan[i].PreGreen > signal_status.plan[arterial_phase].PreGreen) {
            branch_phase = arterial_phase;
            arterial_phase = i;
        } else if (branch_phase == 255 || signal_status.plan[i].PreGreen > signal_status.plan[branch_phase].PreGreen) {
            branch_phase = i;
        }
    }

    LOG_MSG_APPEND(log_content, "arterial_phase:%d \n branch_phase:%d\n", arterial_phase + 1, branch_phase + 1);

    if (total_compensation_time < 0) {  // 進行負補償
        LOG_MSG_APPEND(log_content, "minus compensation\n");
        phase_weight[branch_phase] = 100;
    } else if (total_compensation_time > 0) {  // 進行正補償
        LOG_MSG_APPEND(log_content, "positive compensation\n");
        phase_weight[arterial_phase] = 100;
    }

    implement_compensation_by_weight(&signal_status, log_content, cycle_compensations, Comp_cyclenum, phase_weight);
}

void start_compensation()
{
    char log_content[LOG_CONTENT_LEN + 1] = {0};

    pthread_mutex_lock(&mutex_compensation);
    for (int i = 0; i < SUBPHASEID_NUM; i++)
        LOG_MSG_APPEND(log_content, "compensation_buffer[%d]:%d\n", i, compensation_buffer[i]);
    pthread_mutex_unlock(&mutex_compensation);

    switch (config.traffic_compensation_method) {
    case 0:
        LOG_MSG_APPEND(log_content, "start compensation 0\n Do nothing. TC automatic compensation.\n");
        goto NotReportEnd;
        break;
    case 1:
        traffic_compensation_method1(log_content, config.traffic_compensation_cycle_number);
        break;
    case 2:
        traffic_compensation_method2(log_content, config.traffic_compensation_cycle_number, config.phase_weight);
        break;
    case 3:
        traffic_compensation_method3(log_content, config.traffic_compensation_cycle_number);
        break;
    default:
        break;
    }
    report_compensation_time();
NotReportEnd:
    // 補償結束清空 compensation buffer
    compensation_buffer_clear();
    LOG_MSG_INFO(log_content);
}