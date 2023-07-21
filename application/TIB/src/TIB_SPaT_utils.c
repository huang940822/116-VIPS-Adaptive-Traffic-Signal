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

int spat_msg_update(SPAT *pp_spat)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    if (signal_status.SubPhaseID == 0 || signal_status.StepID == 0)
        return -1;
    IntersectionState *int_state = pp_spat->intersections.tab;
    int_state->revision = (int_state->revision + 1) & 0b1111111;

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);

    int_state->timeStamp_option = TRUE;
    int_state->timeStamp = timeinfo->tm_sec * 1000;
    int_state->moy_option = TRUE;
    int_state->moy = (((timeinfo->tm_yday * 24) + timeinfo->tm_hour) * 60) + timeinfo->tm_min;


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