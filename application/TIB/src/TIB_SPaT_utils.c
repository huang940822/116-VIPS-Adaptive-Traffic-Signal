#include "TIB_SPaT_utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#define RroundHeadGreen 0b00000100
#define LeftGreen 0b00001000
#define StrightGreen 0b00010000
#define RightGreen 0b00100000
const uint8_t signal_mask_arr[] = {RroundHeadGreen, LeftGreen, StrightGreen, RightGreen};

#define after_cur_step(_eventState, _signalType)                           \
    do {                                                                   \
        index = state->state_time_speed.count++;                           \
        state->state_time_speed.tab[index].eventState = _eventState;       \
        state->state_time_speed.tab[index].timing.startTime_option = TRUE; \
        state->state_time_speed.tab[index].timing.startTime = offset;      \
        printf("%d ", offset);                                             \
        offset += signal_status->plan[i]._signalType;                      \
        state->state_time_speed.tab[index].timing.minEndTime = offset;     \
        printf("%d ,", offset);                                            \
    } while (0)

#define run_a_cycle()                                                   \
    do {                                                                \
        for (int k = (cur_subphase + 1) % signal_status->SubPhaseCount; \
             k != i; k = (k + 1) % signal_status->SubPhaseCount) {      \
            offset += signal_status->plan[k].Green;                     \
            offset += signal_status->plan[k].Yellow;                    \
            offset += signal_status->plan[k].AllRed;                    \
        }                                                               \
    } while (0)

static void inline spat_set_state(MovementState *state, traffic_signal_status_t *signal_status, MovementPhaseState greenType, int i)
{
    state->state_time_speed.count = 0;
    int cur_subphase = signal_status->SubPhaseID - 1;
    if (i == cur_subphase) {
        int index = state->state_time_speed.count++;
        int offset = signal_status->StepSec;
        state->state_time_speed.tab[index].timing.startTime_option = TRUE;
        state->state_time_speed.tab[index].timing.minEndTime = offset;
        printf("%d ", offset);

        if (signal_status->StepID == 1 || signal_status->StepID == 2 || signal_status->StepID == 3) {
            state->state_time_speed.tab[index].eventState = greenType;
            state->state_time_speed.tab[index].timing.startTime = -(signal_status->plan[i].Green - signal_status->StepSec);
            printf("%d ,", -(signal_status->plan[i].Green - signal_status->StepSec));

            after_cur_step(MovementPhaseState_protected_clearance, Yellow);
            after_cur_step(MovementPhaseState_stop_And_Remain, AllRed);
        } else if (signal_status->StepID == 4) {
            state->state_time_speed.tab[index].eventState = MovementPhaseState_protected_clearance;
            state->state_time_speed.tab[index].timing.startTime = -(signal_status->plan[i].Yellow - signal_status->StepSec);
            printf("%d ,", -(signal_status->plan[i].Yellow - signal_status->StepSec));

            after_cur_step(MovementPhaseState_stop_And_Remain, AllRed);
            run_a_cycle();
            after_cur_step(greenType, Green);

        } else if (signal_status->StepID == 5) {
            state->state_time_speed.tab[index].eventState = MovementPhaseState_stop_And_Remain;
            state->state_time_speed.tab[index].timing.startTime = -(signal_status->plan[i].AllRed - signal_status->StepSec);
            printf("%d ,", -(signal_status->plan[i].AllRed - signal_status->StepSec));

            run_a_cycle();
            after_cur_step(greenType, Green);
            after_cur_step(MovementPhaseState_protected_clearance, Yellow);
        }
    } else {
        int index = state->state_time_speed.count++;
        state->state_time_speed.tab[index].eventState = MovementPhaseState_stop_And_Remain;
        uint32_t offset = 0;
        offset += signal_status->StepID >= 1 ? signal_status->plan[cur_subphase].Green : 0;
        offset += signal_status->StepID >= 2 ? signal_status->plan[cur_subphase].PedGreenFlash : 0;
        offset += signal_status->StepID >= 3 ? signal_status->plan[cur_subphase].PedRed : 0;
        offset += signal_status->StepID >= 4 ? signal_status->plan[cur_subphase].Yellow : 0;
        offset += signal_status->StepID >= 5 ? signal_status->plan[cur_subphase].AllRed : 0;

        offset -= signal_status->StepSec;
        offset += signal_status->plan[i].AllRed;
        for (int k = (cur_subphase - 1 + signal_status->SubPhaseCount) % signal_status->SubPhaseCount;
             k != i; k = (k - 1 + signal_status->SubPhaseCount) % signal_status->SubPhaseCount) {
            offset += signal_status->plan[k].Green;
            offset += signal_status->plan[k].Yellow;
            offset += signal_status->plan[k].AllRed;
        }
        state->state_time_speed.tab[index].timing.startTime_option = TRUE;
        state->state_time_speed.tab[index].timing.startTime = -offset;
        printf("*%d ", -offset);

        offset = signal_status->StepSec;
        offset += signal_status->StepID <= 1 ? signal_status->plan[i].PedGreenFlash : 0;
        offset += signal_status->StepID <= 2 ? signal_status->plan[i].PedRed : 0;
        offset += signal_status->StepID <= 3 ? signal_status->plan[i].Yellow : 0;
        offset += signal_status->StepID <= 4 ? signal_status->plan[i].AllRed : 0;

        run_a_cycle();
        state->state_time_speed.tab[index].timing.minEndTime = offset;
        printf("%d ,", offset);

        after_cur_step(greenType, Green);
        after_cur_step(MovementPhaseState_protected_clearance, Yellow);
    }
    printf("\n");
}

int spat_msg_update(SPAT *pp_spat)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    if (signal_status.SubPhaseID == 0 || signal_status.StepID == 0)
        return -1;

    printf("signal_status.StepSec %d\n", signal_status.StepSec);
    printf("signal_status.StepID %d\n", signal_status.StepID);
    printf("signal_status.SubPhaseID %d\n", signal_status.SubPhaseID);
    printf("signal_status.SubPhaseCount %d\n", signal_status.SubPhaseCount);

    IntersectionState *int_state = pp_spat->intersections.tab;
    int_state->revision = (int_state->revision + 1) & 0b1111111;

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);

    int_state->timeStamp_option = TRUE;
    int_state->timeStamp = timeinfo->tm_sec * 1000;
    int_state->moy_option = TRUE;
    int_state->moy = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;

    MovementList *statesList = &pp_spat->intersections.tab->states;
    statesList->count = 0;

    int signalGroupID = 1;
    int cur_subphase = signal_status.SubPhaseID - 1;
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for (int j = 0; j < signal_status.SignalCount; j++) {
            for (int k = 0; k < sizeof(signal_mask_arr); k++) {
                if (signal_status.phaseorder_plan[i][j].SignalStatus & signal_mask_arr[i] && signalGroupID <= 255) {
                    MovementState *state = &statesList->tab[statesList->count++];
                    state->signalGroup = signalGroupID++;
                    if (k == 0) {
                        printf("222-----\n");
                        spat_set_state(state, &signal_status, MovementPhaseState_protected_Movement_Allowed, i);
                    } else {
                        printf("333-----\n");
                        spat_set_state(state, &signal_status, MovementPhaseState_protected_Movement_Allowed, i);
                    }
                }
            }
        }
    }
    return 1;
}

void print_spat(SPAT **pp_spat)
{
    IntersectionState *int_state = (*pp_spat)->intersections.tab;
    printf("nowtime : %d\n", int_state->timeStamp);
    printf("moy : %d\n", int_state->moy);
    printf("region : %d\n", int_state->id.region);
    printf("id : %d\n", int_state->id.id);
    for (int i = 0; i < int_state->states.count; i++) {
        printf("sigmalGroup : %d\n", int_state->states.tab[i].signalGroup);

        if (int_state->states.tab[i].state_time_speed.tab[0].timing_option) {
            if (int_state->states.tab[i]
                    .state_time_speed.tab[0]
                    .timing.startTime_option)
                printf("green startTime : %d\n", int_state->states.tab[i]
                                                     .state_time_speed.tab[0]
                                                     .timing.startTime);
            printf("green minEndTime : %d\n", int_state->states.tab[i]
                                                  .state_time_speed.tab[0]
                                                  .timing.minEndTime);
            printf("\n");
        }

        if (int_state->states.tab[i].state_time_speed.tab[1].timing_option) {
            if (int_state->states.tab[i]
                    .state_time_speed.tab[1]
                    .timing.startTime_option)
                printf("yellow startTime : %d\n", int_state->states.tab[i]
                                                      .state_time_speed.tab[1]
                                                      .timing.startTime);
            printf("yellow minEndTime : %d\n", int_state->states.tab[i]
                                                   .state_time_speed.tab[1]
                                                   .timing.minEndTime);
            printf("\n");
        }

        if (int_state->states.tab[i].state_time_speed.tab[2].timing_option) {
            if (int_state->states.tab[i]
                    .state_time_speed.tab[2]
                    .timing.startTime_option)
                printf("red startTime : %d\n", int_state->states.tab[i]
                                                   .state_time_speed.tab[2]
                                                   .timing.startTime);
            printf("red minEndTime : %d\n", int_state->states.tab[i]
                                                .state_time_speed.tab[2]
                                                .timing.minEndTime);
            printf("\n");
        }
    }
}