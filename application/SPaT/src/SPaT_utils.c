#include "SPaT_utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "log.h"

struct tc_now_time tc_store_time = {
    .Sec = 0,
    .Min = 0,
    .Day = 0,
    .Hour = 0,
    .Month = 0,
    .Year = 0,
};

void dump_mem(void *data, int len)
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

bool compare_time(const traffic_signal_status_t *const signal_status)
{
    if (signal_status->Sec != tc_store_time.Sec ||
        signal_status->Min != tc_store_time.Min ||
        signal_status->Hour != tc_store_time.Hour ||
        signal_status->Day != tc_store_time.Day ||
        signal_status->Month != tc_store_time.Month ||
        signal_status->Year != tc_store_time.Year) {
        tc_store_time.Sec = signal_status->Sec;
        tc_store_time.Min = signal_status->Min;
        tc_store_time.Hour = signal_status->Hour;
        tc_store_time.Day = signal_status->Day;
        tc_store_time.Month = signal_status->Month;
        tc_store_time.Year = signal_status->Year;
        compare_to_tc_time = time(NULL);
        return false;
    }
    return true;
}

int spat_msg_update(SPAT **pp_spat)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    if (get_current_phase() == 0 || get_current_step() == 0)
        return -1;
    IntersectionState *int_state = (*pp_spat)->intersections.tab;
    int_state->revision = (int_state->revision + 1) & 0b1111111;

    if (int_state->states.count != signal_status.SubPhaseCount) {
        int_state->states.count = signal_status.SubPhaseCount;
        for (int i = 0; i < int_state->states.count; i++) {
            /* only support to update 1 state */
            int_state->states.tab[i].signalGroup = i + 1;
            int_state->states.tab[i].state_time_speed.count = 3;
            for (int j = 0; j < 3; j++) {
                int_state->states.tab[i].state_time_speed.tab[j].timing_option =
                    TRUE;
            }
            int_state->states.tab[i].state_time_speed.tab[0].eventState =
                SIGNAL_GREEN;
            int_state->states.tab[i].state_time_speed.tab[1].eventState =
                SIGNAL_YELLOW;
            int_state->states.tab[i].state_time_speed.tab[2].eventState =
                SIGNAL_RED;
        }
    }

    compare_time(&signal_status);
    time_t local_time = time(NULL);
    unsigned int diff_time = (unsigned int) difftime(local_time, compare_to_tc_time);
    unsigned int now_time = tc_store_time.Min * 60 + tc_store_time.Sec + (unsigned int) difftime(local_time, compare_to_tc_time);

    unsigned int moy = (unsigned int) ((unsigned int) ((tc_store_time.Sec + diff_time) / 60) +
                                       calDate(tc_store_time.Year + 1911,
                                               tc_store_time.Month - 1,
                                               tc_store_time.Day - 1) *
                                           24 * 60 +
                                       tc_store_time.Hour * 60 +
                                       tc_store_time.Min);

    if (leapYear(tc_store_time.Year + 1911))
        moy %= 527040;
    else
        moy %= 525600;

    int_state->timeStamp_option = TRUE;
    int_state->timeStamp = (tc_store_time.Sec + diff_time) % 60 * 1000;
    int_state->moy_option = TRUE;
    int_state->moy = moy;

    int second = get_current_second();
    int phase = get_current_phase() - 1;
    int step = get_current_step();
    int Green[signal_status.SubPhaseCount], Yellow[signal_status.SubPhaseCount],
        Red[signal_status.SubPhaseCount];
    int Green_end[signal_status.SubPhaseCount],
        Yellow_end[signal_status.SubPhaseCount],
        Red_end[signal_status.SubPhaseCount];
    int all[signal_status.SubPhaseCount];
    int phase_plan_num[signal_status.SubPhaseCount];
    // all
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        phase_plan_num[i] = (signal_status.plan[i].Yellow != 0) ? 3 : 2;
        Green[i] = Green_end[i] = Yellow[i] = Yellow_end[i] = Red_end[i] =
            Red[i] = all[i] = 0;
        for (int j = 0; j < signal_status.SubPhaseCount - 1; j++) {
            all[i] +=
                signal_status.plan[(i + j + 1) % signal_status.SubPhaseCount]
                    .AllRed +
                signal_status.plan[(i + j + 1) % signal_status.SubPhaseCount]
                    .Yellow +
                signal_status.plan[(i + j + 1) % signal_status.SubPhaseCount]
                    .Green;
        }
    }

    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        if (!(int_state->states.tab[i].state_time_speed.tab) ||
            int_state->states.tab[i].state_time_speed.count !=
                phase_plan_num[i]) {
            /* only support to update 1 state */
            int_state->states.tab[i].state_time_speed.count =
                phase_plan_num[i];  // for how many plan in this signal
                                    // group(phase)
            for (int j = 0; j < phase_plan_num[i]; j++) {
                int_state->states.tab[i].state_time_speed.tab[j].timing_option =
                    TRUE;
            }
            int_state->states.tab[i].state_time_speed.tab[0].eventState =
                SIGNAL_GREEN;
            if (phase_plan_num[i] == 2) {  // no yellow
                int_state->states.tab[i].state_time_speed.tab[1].eventState =
                    SIGNAL_RED;
            } else {
                int_state->states.tab[i].state_time_speed.tab[1].eventState =
                    SIGNAL_YELLOW;
                int_state->states.tab[i].state_time_speed.tab[2].eventState =
                    SIGNAL_RED;
            }
        }
    }
    // main phase
    if (step == 1 || step == 2) {
        Green_end[phase] = Yellow[phase] =
            second +
            ((step == 1) ? signal_status.plan[phase].PedGreenFlash : 0);
        Green[phase] = Green_end[phase] - signal_status.plan[phase].Green;
        Yellow_end[phase] = Red[phase] =
            Yellow[phase] + signal_status.plan[phase].Yellow;
        Red_end[phase] =
            Red[phase] + signal_status.plan[phase].AllRed + all[phase];
    } else if (step == 4) {
        Green[phase] = Red_end[phase] =
            second + signal_status.plan[phase].AllRed + all[phase];
        Green_end[phase] = Green[phase] + signal_status.plan[phase].Green;
        Yellow_end[phase] = Red[phase] = second;
        Yellow[phase] = Red[phase] - signal_status.plan[phase].Yellow;
    } else if (step == 5) {
        Green[phase] = Red_end[phase] = second + all[phase];
        Green_end[phase] = Yellow[phase] =
            Green[phase] + signal_status.plan[phase].Green;
        Yellow_end[phase] = Yellow[phase] + signal_status.plan[phase].Yellow;
        Red[phase] = second - signal_status.plan[phase].AllRed;
    }
    // other phase
    if (signal_status.SubPhaseCount == 0 || phase >= signal_status.SubPhaseCount)
        return -1;
    for (int i = (phase + 1) % signal_status.SubPhaseCount, j = phase;
         i != phase; i = (i + 1) % signal_status.SubPhaseCount,
             j = (j + 1) % signal_status.SubPhaseCount) {
        Green[i] = Red_end[i] = Red[j] + signal_status.plan[j].AllRed;
        Green_end[i] = Yellow[i] = Green[i] + signal_status.plan[i].Green;
        Yellow_end[i] = Red[i] = Yellow[i] + signal_status.plan[i].Yellow;
    }
    for (int i = (phase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount, j = phase;
         i != phase; i = (i - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount,
             j = (j - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount) {
        Red[i] = Red[j] + signal_status.plan[j].AllRed - all[j] - signal_status.plan[i].AllRed;
    }

    for (int i = 0, j = 0; i < int_state->states.count; i++, j = 0) {
        timemark_t timemark;
        timemark.min = 0;
        timemark.ms = 0;
        // green
        int_state->states.tab[i]
            .state_time_speed.tab[j]
            .timing.startTime_option = TRUE;
        timemark.sec = (unsigned int) (Green[i] + now_time + 3600) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
            to_TimeMark(timemark);
        timemark.sec = (unsigned int) (Green_end[i] + now_time + 3600) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
            to_TimeMark(timemark);
        j++;
        // yellow
        if (phase_plan_num[i] == 3) {
            int_state->states.tab[i]
                .state_time_speed.tab[j]
                .timing.startTime_option = TRUE;
            timemark.sec = (unsigned int) (Yellow[i] + now_time + 3600) % 3600;
            int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
                to_TimeMark(timemark);
            timemark.sec = (unsigned int) (Yellow_end[i] + now_time + 3600) % 3600;
            int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
                to_TimeMark(timemark);
            j++;
        }

        // red
        int_state->states.tab[i]
            .state_time_speed.tab[j]
            .timing.startTime_option = TRUE;
        timemark.sec = (unsigned int) (Red[i] + now_time + 3600) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
            to_TimeMark(timemark);
        timemark.sec = (unsigned int) (Red_end[i] + now_time + 3600) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
            to_TimeMark(timemark);
    }
    return 1;
}

int leapYear(int a)
{
    if ((a % 4 == 0 && a % 100 != 0) || a % 400 == 0)
        return 1;
    return 0;
}

int calDate(int year, int month, int day)
{
    int sum = 0, i;
    int a[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (i = 0; i < month; i++)
        sum += a[i];
    if (leapYear(year) && month > 1)
        sum++;
    sum += day;
    return sum;
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