#include "TIB_utils.h"

void set_signalGroup()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    uint8_t greenSignalMap[8] = {0};
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for (int j = 0; j < signal_status.SignalCount; j++) {
            greenSignalMap[j] |= (signal_status.phaseorder_plan[i][j].SignalStatus & GreenMask);
        }
    }
}