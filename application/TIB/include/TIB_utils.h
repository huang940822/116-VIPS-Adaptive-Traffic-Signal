#ifndef TIB_UTILS_H
#define TIB_UTILS_H

#include "TIB_config.h"

#define GreenSignalTable  \
    X(RroundHeadGreen, 0) \
    X(LeftGreen, 1)       \
    X(StrightGreen, 2)    \
    X(RightGreen, 3)      \
    X(PedestrianGreen, 4)

typedef enum GreenMask {
#define X(a, b) a##Mask = 1 << (b + 2),
    GreenSignalTable
#undef X
} GreenMask_t;

typedef enum GreenIndex {
#define X(a, b) a##Index = b,
    GreenSignalTable
#undef X
        NumOfGreen
} GreenIndex_t;

int get_map_table(traffic_signal_status_t *signal_status, int map_table[COMPASS_NUM]);
int get_greenSignalMap(traffic_signal_status_t *signal_status, uint8_t greenSignalMap[COMPASS_NUM]);

extern const uint8_t signal_mask_arr[NumOfGreen];
extern const uint8_t GreenMask;

#endif