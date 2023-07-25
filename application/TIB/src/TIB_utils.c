#include <stdio.h>

#include "TIB_config.h"
#include "TIB_utils.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

const uint8_t signal_mask_arr[4] = {RroundHeadGreen, LeftGreen, StrightGreen, RightGreen};

int get_signalGroupMap(traffic_signal_status_t *signal_status, uint8_t greenSignalMap[8])
{
    if (signal_status->SubPhaseCount == 0 || signal_status->SignalCount == 0)
        return -1;

    int map_table[COMPASS_NUM] = {0};
    for (uint8_t mask = 1, i = 0, j = 0; mask != 0; mask = mask << 1, j++) {
        if (mask & signal_status->SignalMap) {
            map_table[i++] = j;
        }
    }

    memset(greenSignalMap, 0, sizeof(uint8_t) * 8);
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        for (int j = 0; j < signal_status->SignalCount; j++) {
            greenSignalMap[j] |= (signal_status->phaseorder_plan[i][j].SignalStatus & GreenMask);
        }
    }

    return signal_status->SignalCount;
}