#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "TIB_SPaT_utils.h"
#include "TIB_utils.h"
#include "config.h"
#include "log.h"

SPAT *p_spat;

static void dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *) data;
    for (count = 0; count < len; count++) {
        if (count % 16 == 0)
            printf("\n");

        printf("%02X ", p[count]);
    }
    printf("\n\n");
}

int spat_msg_init(SPAT **pp_spat)
{
    int ret;
    J2735Config cfg;
    unsigned int i;
    ret = j2735_init(&cfg);
    if (ret != 0) {
        log_file_write_fatal_error("J2735 library initial");
        perror("J2735 library initial");
        exit(errno);
    }

    (*pp_spat) = (SPAT *) j2735_msg_prealloc(SPAT_Id);
    /* only 1 intersection */
    (*pp_spat)->intersections.count = 1;
    IntersectionState *int_state = (*pp_spat)->intersections.tab;
    /* set randomly  */
    int_state->id.id = config.RSU_id;
    int_state->id.region_option = TRUE;
    int_state->id.region = config.RSU_region;
    // 訊息流水號 init. to 0
    int_state->revision = 0;

    // 定時控制
    asn1_bstr_set_bit(&(int_state->status), IntersectionStatusObject_fixedTimeOperation);
    return 0;
}

int spat_msg_update(SPAT *pp_spat)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    if (signal_status.SubPhaseID == 0 || signal_status.StepID == 0 ||
        signal_status.SubPhaseCount == 0 || signal_status.SignalCount == 0)
        return -1;

    IntersectionState *int_state = pp_spat->intersections.tab;
    int_state->revision = (int_state->revision + 1) & 0b1111111;

    time_t rawtime;
    time(&rawtime);
    struct tm result;
    struct tm *timeinfo = localtime_r(&rawtime, &result);

    int_state->timeStamp_option = TRUE;
    int_state->timeStamp = timeinfo->tm_sec * 1000;
    int_state->moy_option = TRUE;
    int_state->moy = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;

    MovementList *statesList = &pp_spat->intersections.tab->states;
    statesList->count = 0;  // clean statesList

    uint8_t greenSignalMap[COMPASS_NUM] = {0};
    if (get_signalGroupMap(&signal_status, greenSignalMap) < 0)
        return -1;

#define to_TimeMark(tmark) \
    ((timeinfo->tm_min * 60 + timeinfo->tm_sec + (tmark) + 3600) % 3600) * 10

#define after_cur_step(_eventState, _offset_signalTime, _extra_func)                \
    do {                                                                            \
        index = state->state_time_speed.count++;                                    \
        state->state_time_speed.tab[index].eventState = _eventState;                \
        state->state_time_speed.tab[index].timing_option = TRUE;                    \
        state->state_time_speed.tab[index].timing.startTime_option = TRUE;          \
        printf("%d ", offset);                                                      \
        state->state_time_speed.tab[index].timing.startTime = to_TimeMark(offset);  \
        _offset_signalTime;                                                         \
        _extra_func;                                                                \
        printf("%d\n", offset);                                                     \
        state->state_time_speed.tab[index].timing.minEndTime = to_TimeMark(offset); \
    } while (0)

//
#define increase_offset_red_signal()                                                    \
    do {                                                                                \
        for (int k = (cur_subphase + 1) % signal_status.SubPhaseCount, t = 0;           \
             !(signal_status.phaseorder_plan[k][i].SignalStatus & signal_mask_arr[j] && \
               t < signal_status.SubPhaseCount);                                        \
             k = (k + 1) % signal_status.SubPhaseCount, ++t) {                          \
            offset += signal_status.plan[k].PreGreen;                                   \
            offset += signal_status.plan[k].Yellow;                                     \
            offset += signal_status.plan[k].AllRed;                                     \
            ++subphase_shift;                                                           \
        }                                                                               \
    } while (0)

#define increase_offset_green_signal()                                                 \
    do {                                                                               \
        for (int k = cur_subphase, t = 0;                                              \
             signal_status.plan[k].Yellow == 0 && signal_status.plan[k].AllRed == 0 && \
             t < signal_status.SubPhaseCount;                                          \
             ++t) {                                                                    \
            k = (k + 1) % signal_status.SubPhaseCount;                                 \
            offset += signal_status.plan[k].PreGreen;                                  \
            offset += signal_status.plan[k].PedGreenFlash;                             \
            ++subphase_shift;                                                          \
        }                                                                              \
    } while (0)

    int cur_subphase = signal_status.SubPhaseID - 1;
    int signalGroupID = 1;
    printf("----------\n");
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        printf("plan %d %d %d %d %d %d\n", i + 1, signal_status.plan[i].PreGreen, signal_status.plan[i].PedGreenFlash,
               signal_status.plan[i].PedRed, signal_status.plan[i].Yellow, signal_status.plan[i].AllRed);
    }
    printf("PhaseOrder %d\n", signal_status.PhaseOrder);
    printf("SubPhaseID %d\n", signal_status.SubPhaseID);
    printf("StepID %d\n", signal_status.StepID);
    printf("StepSec %d\n", signal_status.StepSec);
    for (int i = 0; i < signal_status.SignalCount; i++) {
        for (int j = 0; j < sizeof(signal_mask_arr); j++) {
            if (greenSignalMap[i] & signal_mask_arr[j] && signalGroupID <= 255) {
                MovementState *state = &statesList->tab[statesList->count++];
                int index = 0;
                state->state_time_speed.count = 0;
                state->signalGroup = signalGroupID++;
                // 是不是圓頭綠
                MovementPhaseState greenType = j == 0 ? MovementPhaseState_permissive_Movement_Allowed : MovementPhaseState_protected_Movement_Allowed;
                int offset = 0, subphase_shift = 0;

                if (signal_status.phaseorder_plan[cur_subphase][i].SignalStatus & signal_mask_arr[j]) {
                    printf("---++\n");
                    int green_offset = 0;
                    for (int k = (cur_subphase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount, t = 0;
                         signal_status.plan[k].Yellow == 0 && signal_status.plan[k].AllRed == 0 && t < signal_status.SubPhaseCount;
                         ++t) {
                        k = (k - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount;
                        offset += signal_status.plan[k].PreGreen;
                        offset += signal_status.plan[k].PedGreenFlash;
                    }
                    switch (signal_status.StepID) {
                    case 1:
                        offset += signal_status.plan[cur_subphase].PreGreen;
                        green_offset += signal_status.plan[cur_subphase].PedGreenFlash;
                    case 2:
                        offset += signal_status.plan[cur_subphase].PedGreenFlash;
                        green_offset += signal_status.plan[cur_subphase].PedRed;
                    case 3:
                        offset += signal_status.plan[cur_subphase].PedRed;
                        offset = signal_status.StepSec - offset;

                        printf("green %d ", subphase_shift);
                        after_cur_step(greenType, offset = signal_status.StepSec + green_offset, increase_offset_green_signal());
                        printf("yellow %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase + subphase_shift].Yellow, );
                        printf("red %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase + subphase_shift].AllRed, increase_offset_red_signal());
                        break;
                    case 4:
                        offset = -(signal_status.plan[cur_subphase].Yellow - signal_status.StepSec);

                        printf("yellow %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_protected_clearance, offset = signal_status.StepSec, );
                        printf("red %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase + subphase_shift].AllRed, increase_offset_red_signal());
                        printf("green %d ", subphase_shift);
                        after_cur_step(greenType, offset += signal_status.plan[cur_subphase + subphase_shift].Green, increase_offset_green_signal());
                        break;
                    case 5:
                        offset = -(signal_status.plan[cur_subphase].AllRed - signal_status.StepSec);

                        printf("red %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_stop_And_Remain, offset = signal_status.StepSec;, increase_offset_red_signal());
                        printf("green %d ", subphase_shift);
                        after_cur_step(greenType, offset += signal_status.plan[cur_subphase + subphase_shift].Green, increase_offset_green_signal());
                        printf("yellow %d ", subphase_shift);
                        after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase + subphase_shift].Yellow, );
                    }
                } else {
                    printf("---\n");
                    state->state_time_speed.count++;
                    state->state_time_speed.tab[index].eventState = MovementPhaseState_stop_And_Remain;
                    offset += signal_status.StepID >= 1 ? signal_status.plan[cur_subphase].PreGreen : 0;
                    offset += signal_status.StepID >= 2 ? signal_status.plan[cur_subphase].PedGreenFlash : 0;
                    offset += signal_status.StepID >= 3 ? signal_status.plan[cur_subphase].PedRed : 0;
                    offset += signal_status.StepID >= 4 ? signal_status.plan[cur_subphase].Yellow : 0;
                    offset += signal_status.StepID >= 5 ? signal_status.plan[cur_subphase].AllRed : 0;

                    offset -= signal_status.StepSec;
                    offset += signal_status.plan[i].AllRed;
                    for (int k = (cur_subphase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount;
                         k != i; k = (k - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount) {
                        offset += signal_status.plan[k].PreGreen;
                        offset += signal_status.plan[k].Yellow;
                        offset += signal_status.plan[k].AllRed;
                    }
                    state->state_time_speed.tab[index].timing.startTime_option = TRUE;
                    printf("red %d %d ", subphase_shift , -offset);
                    state->state_time_speed.tab[index].timing.startTime = to_TimeMark(-offset);

                    offset = signal_status.StepSec;
                    offset += signal_status.StepID <= 1 ? signal_status.plan[i].PedGreenFlash : 0;
                    offset += signal_status.StepID <= 2 ? signal_status.plan[i].PedRed : 0;
                    offset += signal_status.StepID <= 3 ? signal_status.plan[i].Yellow : 0;
                    offset += signal_status.StepID <= 4 ? signal_status.plan[i].AllRed : 0;

                    increase_offset_red_signal();
                    printf("%d\n", offset);
                    state->state_time_speed.tab[index].timing.minEndTime = to_TimeMark(offset);
                    printf("green %d ", subphase_shift);
                    after_cur_step(greenType, offset += signal_status.plan[cur_subphase + subphase_shift].Green, increase_offset_green_signal());
                    printf("yellow %d ", subphase_shift);
                    after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase + subphase_shift].Yellow, );
                }
            }
        }
    }
    if (statesList->count == 0)
        return -1;
    return 1;
}

void spat_printf(SPAT *pp_spat)
{
    IntersectionState *int_state = pp_spat->intersections.tab;
    printf("nowtime : %d\n", int_state->timeStamp);
    printf("moy : %d\n", int_state->moy);
    printf("region : %d\n", int_state->id.region);
    printf("id : %d\n", int_state->id.id);
    for (int i = 0; i < int_state->states.count; i++) {
        MovementState *state = &int_state->states.tab[i];
        printf("\nsigmalGroup : %d %d\n", state->signalGroup, state->state_time_speed.count);

        for (int j = 0; j < state->state_time_speed.count; j++) {
            if (state->state_time_speed.tab[j].timing_option == TRUE) {
                if (state->state_time_speed.tab[j].eventState == MovementPhaseState_permissive_Movement_Allowed)
                    printf("round green\n");
                else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_protected_Movement_Allowed)
                    printf("arrow green\n");
                else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_protected_clearance)
                    printf("yallow\n");
                else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_stop_And_Remain)
                    printf("red\n");
                printf(" startTime : %d ", state->state_time_speed.tab[j].timing.startTime);
                printf(" minEndTime : %d\n", state->state_time_speed.tab[j].timing.minEndTime);
            }
        }
    }
}