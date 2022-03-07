#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SPaT_utils.h"
#include "log.h"

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

    (*pp_spat) = (SPAT *) calloc(1, sizeof(SPAT));
    /* only 1 intersection */
    (*pp_spat)->intersections.count = 1;
    (*pp_spat)->intersections.tab =
        (IntersectionState *) calloc(1, sizeof(IntersectionState));
    IntersectionState *int_state = (*pp_spat)->intersections.tab;
    /* set randomly  */
    int_state->id.id = 4009;
    /* init. to 0 */
    int_state->revision = 0;
    asn1_bstr_alloc(&(int_state->status), IntersectionStatusObject_MAX_BITS);
    asn1_bstr_set_bit(&(int_state->status),
                      IntersectionStatusObject_fixedTimeOperation);
    int_state->states.count = SPaT_config.signalcount;
    int_state->states.tab = (MovementState *) calloc(SPaT_config.signalcount,
                                                     sizeof(MovementState));
    for (i = 0; i < SPaT_config.signalcount; i++) {
        /* only support to update 1 state */
        int_state->states.tab[i].signalGroup = i;
        int_state->states.tab[i].state_time_speed.count = 3;
        int_state->states.tab[i].state_time_speed.tab =
            (MovementEvent *) calloc(3, sizeof(MovementEvent));
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

    return 0;
}

int compose_spat(uint8_t **pp_spat_buf, SPAT *p_spat)
{
    int buf_len;
    J2735CodecErr err;

    char log_content[LOG_CONTENT_LEN + 1];
    char errmsg_buf[ERR_MSG_SZ];

    MessageFrame msgf;
    memset(&msgf, 0, sizeof(msgf));

    memset(&err, 0, sizeof(J2735CodecErr));
    err.msg_size = ERR_MSG_SZ;
    err.msg = errmsg_buf;

    msgf.messageId = SPAT_Id;
    msgf.u.data = p_spat;
    buf_len = j2735_msg_encode(pp_spat_buf, &msgf, &err);

    if (buf_len <= 0) {
        printf("failed to encode SPAT msg\n");
        printf("  [error msg] %s\n", err.msg);
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "failed to encode SPAT msg\r\n");
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "  [error msg] %s \r\n",
                 err.msg);
        log_file_write(log_content);
    }
    return buf_len;
}

int spat_msg_update(SPAT **pp_spat)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    if (get_current_phase() - 1 < 0 || get_current_step() == 0)
        return -1;
    IntersectionState *int_state = (*pp_spat)->intersections.tab;
    int now_time = signal_status.Min * 60 + signal_status.Sec;
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

    // calloc the memory of step in each signal group
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        if (!(int_state->states.tab[i].state_time_speed.tab) ||
            int_state->states.tab[i].state_time_speed.count !=
                phase_plan_num[i]) {
            /* only support to update 1 state */
            int_state->states.tab[i].state_time_speed.count =
                phase_plan_num[i];  // for how many plan in this signal
                                    // group(phase)
            int_state->states.tab[i].state_time_speed.tab =
                (MovementEvent *) calloc(phase_plan_num[i],
                                         sizeof(MovementEvent));
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
        Green[phase] = 0;
        Green_end[phase] = Yellow[phase] =
            second +
            ((step == 1) ? signal_status.plan[phase].PedGreenFlash : 0);
        Yellow_end[phase] = Red[phase] =
            Yellow[phase] + signal_status.plan[phase].Yellow;
        Red_end[phase] =
            Red[phase] + signal_status.plan[phase].AllRed + all[phase];
    } else if (step == 4) {
        Green[phase] = Red_end[phase] =
            second + signal_status.plan[phase].AllRed + all[phase];
        Green_end[phase] = Green[phase] + signal_status.plan[phase].Green;
        Yellow[phase] = 0;
        Yellow_end[phase] = Red[phase] = second;
    } else if (step == 5) {
        Green[phase] = Red_end[phase] = second + all[phase];
        Green_end[phase] = Yellow[phase] =
            Green[phase] + signal_status.plan[phase].Green;
        Yellow_end[phase] = Yellow[phase] + signal_status.plan[phase].Yellow;
        Red[phase] = second - signal_status.plan[phase].AllRed;
    }
    // other phase
    for (int i = (phase + 1) % signal_status.SubPhaseCount, j = phase;
         i != phase; i = (i + 1) % signal_status.SubPhaseCount,
             j = (j + 1) % signal_status.SubPhaseCount) {
        Green[i] = Red_end[i] = Red[j] + signal_status.plan[j].AllRed;
        Green_end[i] = Yellow[i] = Green[i] + signal_status.plan[i].Green;
        Yellow_end[i] = Red[i] = Yellow[i] + signal_status.plan[i].Yellow;
    }
    for (int i = 0, j = 0; i < int_state->states.count; i++, j = 0) {
        timemark_t timemark;
        timemark.min = 0;
        timemark.ms = 0;
        // green
        int_state->states.tab[i]
            .state_time_speed.tab[j]
            .timing.startTime_option = TRUE;
        timemark.sec = (unsigned int) (Green[i] + now_time) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
            to_TimeMark(timemark);
        timemark.sec = (unsigned int) (Green_end[i] + now_time) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
            to_TimeMark(timemark);
        j++;
        // yellow
        if (phase_plan_num[i] == 3) {
            int_state->states.tab[i]
                .state_time_speed.tab[j]
                .timing.startTime_option = TRUE;
            timemark.sec = (unsigned int) (Yellow[i] + now_time) % 3600;
            int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
                to_TimeMark(timemark);
            timemark.sec = (unsigned int) (Yellow_end[i] + now_time) % 3600;
            int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
                to_TimeMark(timemark);
            j++;
        }

        // red
        if (i == phase) {
            int_state->states.tab[i]
                .state_time_speed.tab[j]
                .timing.startTime_option = TRUE;
            timemark.sec = (unsigned int) (Red[i] + now_time) % 3600;
            int_state->states.tab[i].state_time_speed.tab[j].timing.startTime =
                to_TimeMark(timemark);
        } else
            int_state->states.tab[i]
                .state_time_speed.tab[j]
                .timing.startTime_option = FALSE;
        timemark.sec = (unsigned int) (Red_end[i] + now_time) % 3600;
        int_state->states.tab[i].state_time_speed.tab[j].timing.minEndTime =
            to_TimeMark(timemark);
    }
    // for main phase, we do not give it main step starttime, because it may be
    // wrong.
    if (step == 1 || step == 2) {
        int_state->states.tab[phase]
            .state_time_speed.tab[0]
            .timing.startTime_option = FALSE;
    } else if (step == 4) {
        int_state->states.tab[phase]
            .state_time_speed.tab[1]
            .timing.startTime_option = FALSE;
    } else if (step == 5) {
        int_state->states.tab[phase]
            .state_time_speed.tab[(phase_plan_num[phase] == 3) ? 2 : 1]
            .timing.startTime_option = FALSE;
    }
    // printf("time : %d\n", now_time * 10);
    // print_spat(pp_spat);
    return 1;
}

static void print_spat(SPAT **pp_spat)
{
    IntersectionState *int_state = (*pp_spat)->intersections.tab;

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
        /*
            if(int_state->states.tab[i].state_time_speed.tab[0].speeds.tab[0].speed_option)
                printf("advitoryspeed : %d\n",
           int_state->states.tab[i].state_time_speed.tab[0].speeds.tab[0].speed);

            if(int_state->states.tab[i].state_time_speed.tab->speeds.tab->distance_option)
                printf("zonedistance : %d\n",
           int_state->states.tab[i].state_time_speed.tab->speeds.tab->distance);


            printf("connectionID : %d\n\n",
           int_state->states.tab[i].maneuverAssistList.tab-> connectionID);
        */
    }
}