#include <pthread.h>
#include <stdio.h>

#include "TIB_config.h"
#include "TIB_utils.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

#define X(a, b) a##Mask,
const uint8_t signal_mask_arr[] = {GreenSignalTable};
#undef X

#define X(a, b) a##Mask |
const uint8_t GreenMask = (GreenSignalTable 0);
#undef X

pthread_mutex_t adjust_time_mutex = PTHREAD_MUTEX_INITIALIZER;

int adjust_time;

int get_map_table(traffic_signal_status_t *signal_status, int map_table[COMPASS_NUM])
{
    if (signal_status->SubPhaseCount == 0 || signal_status->SignalCount == 0)
        return -1;

    memset(map_table, 0, COMPASS_NUM * sizeof(int));
    for (uint8_t mask = 1, i = 0, j = 0; mask != 0; mask = mask << 1, j++) {
        if (mask & signal_status->SignalMap) {
            map_table[i++] = j;
        }
    }
    return 1;
}

// 確定每個方向的燈號在號控器設定中有哪些綠燈，像是直綠 右綠 圓頭綠等
int get_greenSignalMap(traffic_signal_status_t *signal_status, uint8_t greenSignalMap[COMPASS_NUM])
{
    int map_table[COMPASS_NUM];
    if (signal_status->SubPhaseCount == 0 || signal_status->SignalCount == 0 ||
        get_map_table(signal_status, map_table) < 0)
        return -1;

    memset(greenSignalMap, 0, sizeof(uint8_t) * 8);
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        for (int j = 0; j < signal_status->SignalCount; j++) {
            greenSignalMap[j] |= (signal_status->phaseorder_plan[i][j].SignalStatus & GreenMask);
        }
    }
    // 在驗證綠燈的時候可以使用這段 他只會讓 SPaT 傳有綠燈的方向
    // for (int j = 0; j < signal_status->SignalCount; j++) {
    //     greenSignalMap[j] |= (signal_status->phaseorder_plan[signal_status->SubPhaseID - 1][j].SignalStatus & GreenMask);
    // }

    return signal_status->SignalCount;
}

void set_adjust_time(int time)
{
    pthread_mutex_lock(&adjust_time_mutex);
    adjust_time = time;
    pthread_mutex_unlock(&adjust_time_mutex);
}

int get_adjust_time()
{
    pthread_mutex_lock(&adjust_time_mutex);
    int tmp = adjust_time;
    adjust_time = 0;
    pthread_mutex_unlock(&adjust_time_mutex);
    return tmp;
}