#ifndef TIB_UTILS_H
#define TIB_UTILS_H

#include "TIB_config.h"

#define RroundHeadGreen 0b00000100
#define LeftGreen 0b00001000
#define StrightGreen 0b00010000
#define RightGreen 0b00100000

#define GreenMask (RroundHeadGreen | LeftGreen | StrightGreen | RightGreen)

int get_map_table(traffic_signal_status_t *signal_status, int map_table[COMPASS_NUM]);
int get_greenSignalMap(traffic_signal_status_t *signal_status, uint8_t greenSignalMap[COMPASS_NUM]);

extern const uint8_t signal_mask_arr[4];

#endif