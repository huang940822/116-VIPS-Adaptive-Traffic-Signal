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

    return signal_status->SignalCount;
}