#ifndef TIB_UTILS_H
#define TIB_UTILS_H

#include "typedefine.h"

#define COMPASS_NUM 8
// const char *conpass_order[] = COMPASS_ORDER;
#define COMPASS_ORDER                              \
    {                                              \
        "N", "NE", "E", "SE", "S", "SW", "W", "NW" \
    }

#define GreenSignalTable  \
    X(RroundHeadGreen, 0) \
    X(LeftGreen, 1)       \
    X(StraightGreen, 2)    \
    X(RightGreen, 3)      \
    X(PedestrianGreen, 4)

typedef enum GreenMask {
    RedMask = 1,
    YellowMask = 2,
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
void set_adjust_time(int time);
int get_adjust_time();

extern const uint8_t signal_mask_arr[NumOfGreen];
extern const uint8_t GreenMask;

#endif