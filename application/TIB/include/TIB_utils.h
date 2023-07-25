#ifndef TIB_UTILS_H
#define TIB_UTILS_H

#define RroundHeadGreen 0b00000100
#define LeftGreen 0b00001000
#define StrightGreen 0b00010000
#define RightGreen 0b00100000

#define GreenMask (RroundHeadGreen | LeftGreen | StrightGreen | RightGreen)

typedef struct TIB_SignalGroupID {
    int signalIndex; // 按照 5F3C 的順序
    int signalGroupID;
    uint8_t compass;
} TIB_SignalGroupID_t;

int get_signalGroupMap(traffic_signal_status_t *signal_status, uint8_t greenSignalMap[8]);

extern const uint8_t signal_mask_arr[4];

#endif