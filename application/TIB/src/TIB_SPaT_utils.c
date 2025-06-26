#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "TIB_SPaT_utils.h"
#include "TIB_utils.h"
#include "config.h"
#include "log.h"

LOG_USE_MODULE(TIB);

SPAT *p_spat;
Reg_IntersectionState reg_spat[1];

// #define SPaT_debug(...) LOG_MSG_TRACE(__VA_ARGS__)
#define SPaT_debug(...) ;

static void dump_mem(void *data, int len)
{
    int count;
    unsigned char *p = (unsigned char *) data;
    char data_str[LOG_CONTENT_LEN + 1];
    memset(data_str, 0, sizeof(data_str));
    for (count = 0; count < len; count++) {
        if (count % 16 == 0)
            LOG_MSG_APPEND(data_str, "\n");

        LOG_MSG_APPEND(data_str, "%02X ", p[count]);
    }
    LOG_MSG_TRACE(data_str);
}

int spat_msg_init(SPAT **pp_spat)
{
    int ret;
    J2735Config cfg;
    unsigned int i;
    ret = j2735_init(&cfg);
    if (ret != 0) {
        LOG_MSG_FATAL("J2735 library initial");
        perror("J2735 library initial");
        exit(errno);
    }
    // 不用 j2735_msg_prealloc 是因為 MovementState 幫忙宣告的最多到 16 個而已
    Malloc((*pp_spat), sizeof(SPAT), "SPaT msg");
    // 因為只有一個路口所以只宣告一個路口的
    Malloc((*pp_spat)->intersections.tab, sizeof(IntersectionState), "SPaT IntersectionState");
    Malloc((*pp_spat)->intersections.tab[0].states.tab, sizeof(MovementState) * MovementList_MAX_SIZE, "SPaT MovementState");
    for (int i = 0; i < MovementList_MAX_SIZE; i++) {
        Malloc((*pp_spat)->intersections.tab[0].states.tab[i].state_time_speed.tab, sizeof(MovementEvent) * MovementList_MAX_SIZE, "SPaT MovementState");
    }
    Malloc((*pp_spat)->intersections.tab[0].status.buf, 16, "SPaT BitString status");
    (*pp_spat)->intersections.tab[0].status.len = 16;

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
    if (get_greenSignalMap(&signal_status, greenSignalMap) < 0)
        return -1;

#define to_TimeMark(tmark) \
    ((timeinfo->tm_min * 60 + timeinfo->tm_sec + (tmark) + 3600) % 3600) * 10
#define next_subphase(subPhaseId) ((subPhaseId + 1) % signal_status.SubPhaseCount)

#define after_cur_step(_eventState, _offset_signalTime, _extra_func)                \
    do {                                                                            \
        index = state->state_time_speed.count++;                                    \
        state->state_time_speed.tab[index].eventState = _eventState;                \
        state->state_time_speed.tab[index].timing_option = TRUE;                    \
        state->state_time_speed.tab[index].timing.startTime_option = TRUE;          \
        SPaT_debug("%d ", offset);                                                  \
        state->state_time_speed.tab[index].timing.startTime = to_TimeMark(offset);  \
        _offset_signalTime;                                                         \
        _extra_func;                                                                \
        SPaT_debug("%d\n", offset);                                                 \
        state->state_time_speed.tab[index].timing.minEndTime = to_TimeMark(offset); \
    } while (0)

//
#define increase_offset_red_signal()                                                     \
    do {                                                                                 \
        for (int k = next_subphase(cur_subphase), t = 0;                                 \
             !(signal_status.phaseorder_plan[k][i].SignalStatus & signal_mask_arr[j]) && \
             t < signal_status.SubPhaseCount;                                            \
             k = (k + 1) % signal_status.SubPhaseCount, ++t) {                           \
            offset += signal_status.plan[k].Green;                                       \
            offset += signal_status.plan[k].Yellow;                                      \
            offset += signal_status.plan[k].AllRed;                                      \
            cur_subphase = next_subphase(cur_subphase);                                  \
        }                                                                                \
    } while (0)

#define leading_subphase_signal()                                                      \
    do {                                                                               \
        for (int k = cur_subphase, t = 0;                                              \
             signal_status.plan[k].Yellow == 0 && signal_status.plan[k].AllRed == 0 && \
             t < signal_status.SubPhaseCount;                                          \
             ++t) {                                                                    \
            k = (k + 1) % signal_status.SubPhaseCount;                                 \
            offset += signal_status.plan[k].Green;                                     \
            cur_subphase = next_subphase(cur_subphase);                                \
        }                                                                              \
    } while (0)

#define lagging_subphase_signal()                                                                    \
    do {                                                                                             \
        for (int k = cur_subphase, t = 0;                                                            \
             signal_status.plan[k].AllRed == 0 && signal_status.plan[k].Yellow != 0 &&               \
             signal_status.phaseorder_plan[next_subphase(k)][i].SignalStatus & signal_mask_arr[j] && \
             t < signal_status.SubPhaseCount;                                                        \
             ++t) {                                                                                  \
            k = (k + 1) % signal_status.SubPhaseCount;                                               \
            offset += (signal_status.plan[k].Green + signal_status.plan[k].Yellow);                  \
            cur_subphase = next_subphase(cur_subphase);                                              \
        }                                                                                            \
    } while (0)

    if (get_greenSignalMap(&signal_status, greenSignalMap) < 0)
        return -1;

    SPaT_debug("----------\n");
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        SPaT_debug("plan %d %d %d %d %d %d\n", i + 1, signal_status.plan[i].PreGreen, signal_status.plan[i].PedGreenFlash,
                   signal_status.plan[i].PedRed, signal_status.plan[i].Yellow, signal_status.plan[i].AllRed);
    }
    SPaT_debug("PhaseOrder %d\n", signal_status.PhaseOrder);
    SPaT_debug("SubPhaseID %d\n", signal_status.SubPhaseID);
    SPaT_debug("StepID %d\n", signal_status.StepID);
    SPaT_debug("StepSec %d\n", signal_status.StepSec);

    static int adjust_time = 0;
    static uint8_t adjust_flag = 0;
    static uint8_t pre_signal_table[8] = {0};
    static int preSec = 0;
    uint8_t signal_table[8] = {0};
    for (int i = 0; i < signal_status.SignalCount; i++) {
        for (int j = 0; j < sizeof(signal_mask_arr); j++) {
            if (greenSignalMap[i] & signal_mask_arr[j]) {
                for (int k = 0; k < vector_size(TIB_config.signalId_table[i][j]); k++) {
                    MovementState *state = &statesList->tab[statesList->count++];
                    MovementPhaseState greenType, yellowType;
                    int index = 0, cur_subphase = signal_status.SubPhaseID - 1, offset = 0;

                    state->state_time_speed.count = 0;
                    signalID_obj_t *sGobj = &vector_at(TIB_config.signalId_table[i][j], k);
                    state->signalGroup = sGobj->signalGroupID;

                    if (sGobj->signalGreenType != PedestrianGreenIndex) {  // 行車綠
                        // 圓頭 MovementPhaseState_permissive_Movement_Allowed, 箭頭 MovementPhaseState_protected_Movement_Allowed
                        greenType = (sGobj->signalGreenType == RroundHeadGreenIndex) ? MovementPhaseState_permissive_Movement_Allowed : MovementPhaseState_protected_Movement_Allowed;
                        if (signal_status.phaseorder_plan[cur_subphase][i].SignalStatus & signal_mask_arr[j]) {
                            SPaT_debug("---++\n");
                            // 早開
                            for (int k = (cur_subphase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount, t = 0;
                                 signal_status.phaseorder_plan[k][i].SignalStatus & signal_mask_arr[j] &&
                                 signal_status.plan[k].AllRed == 0 && t < signal_status.SubPhaseCount;
                                 ++t) {
                                offset += signal_status.plan[k].Green;
                                k = (k - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount;
                            }

                            int green_offset = signal_status.plan[cur_subphase].PedGreenFlash + signal_status.plan[cur_subphase].PedRed;
                            switch (signal_status.StepID) {
                            case 3:
                                offset += signal_status.plan[cur_subphase].PedRed;
                                green_offset = 0;
                            case 2:
                                offset += signal_status.plan[cur_subphase].PedGreenFlash;
                                green_offset -= signal_status.plan[cur_subphase].PedGreenFlash;
                            case 1:
                                offset += signal_status.plan[cur_subphase].PreGreen;
                                offset = signal_status.StepSec - offset;

                                SPaT_debug("green %d ", cur_subphase);
                                after_cur_step(greenType, offset = signal_status.StepSec + green_offset, leading_subphase_signal(); lagging_subphase_signal(););
                                SPaT_debug("yellow %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].Yellow, );
                                SPaT_debug("red %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase].AllRed, increase_offset_red_signal());

                                signal_table[i] |= RroundHeadGreenMask;  // 綠燈
                                break;
                            case 4:
                                // 遲閉
                                if (signal_status.plan[cur_subphase].AllRed == 0 && signal_status.plan[cur_subphase].Yellow != 0 &&
                                    signal_status.phaseorder_plan[next_subphase(cur_subphase)][i].SignalStatus & signal_mask_arr[j]) {
                                    offset += signal_status.plan[cur_subphase].PedRed;
                                    offset += signal_status.plan[cur_subphase].PedGreenFlash;
                                    offset += signal_status.plan[cur_subphase].PreGreen;
                                    offset = signal_status.StepSec - offset;

                                    cur_subphase = next_subphase(cur_subphase);
                                    SPaT_debug("green %d ", cur_subphase);
                                    after_cur_step(greenType, offset = signal_status.StepSec + signal_status.plan[cur_subphase].Green, leading_subphase_signal(); lagging_subphase_signal(););
                                    SPaT_debug("yellow %d ", cur_subphase);
                                    after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].Yellow, );
                                    SPaT_debug("red %d ", cur_subphase);
                                    after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase].AllRed, increase_offset_red_signal());

                                    signal_table[i] |= RroundHeadGreenMask;  // 綠燈
                                } else {
                                    offset = -(signal_status.plan[cur_subphase].Yellow - signal_status.StepSec);

                                    SPaT_debug("yellow %d ", cur_subphase);
                                    after_cur_step(MovementPhaseState_protected_clearance, offset = signal_status.StepSec, );
                                    SPaT_debug("red %d ", cur_subphase);
                                    after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase].AllRed, increase_offset_red_signal());
                                    SPaT_debug("green %d ", cur_subphase);
                                    after_cur_step(greenType, offset += signal_status.plan[cur_subphase].Green, leading_subphase_signal(); lagging_subphase_signal(););

                                    signal_table[i] |= YellowMask;  // 黃燈
                                }
                                break;
                            case 5:
                                offset = -(signal_status.plan[cur_subphase].AllRed - signal_status.StepSec);

                                SPaT_debug("red %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_stop_And_Remain, offset = signal_status.StepSec;, increase_offset_red_signal());
                                SPaT_debug("green %d ", cur_subphase);
                                after_cur_step(greenType, offset += signal_status.plan[cur_subphase].Green, leading_subphase_signal(); lagging_subphase_signal(););
                                SPaT_debug("yellow %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].Yellow, );

                                signal_table[i] |= RedMask;  // 紅燈
                            }
                        } else {
                            SPaT_debug("---\n");
                            state->state_time_speed.count++;
                            state->state_time_speed.tab[index].eventState = MovementPhaseState_stop_And_Remain;
                            offset += signal_status.StepID >= 1 ? signal_status.plan[cur_subphase].PreGreen : 0;
                            offset += signal_status.StepID >= 2 ? signal_status.plan[cur_subphase].PedGreenFlash : 0;
                            offset += signal_status.StepID >= 3 ? signal_status.plan[cur_subphase].PedRed : 0;
                            offset += signal_status.StepID >= 4 ? signal_status.plan[cur_subphase].Yellow : 0;
                            offset += signal_status.StepID >= 5 ? signal_status.plan[cur_subphase].AllRed : 0;

                            offset -= signal_status.StepSec;
                            offset += signal_status.plan[cur_subphase].AllRed;
                            for (int k = (cur_subphase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount;
                                 !(signal_status.phaseorder_plan[k][i].SignalStatus & signal_mask_arr[j]);
                                 k = (k - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount) {
                                offset += signal_status.plan[k].Green;
                                offset += signal_status.plan[k].Yellow;
                                offset += signal_status.plan[k].AllRed;
                            }
                            state->state_time_speed.tab[index].timing.startTime_option = TRUE;
                            SPaT_debug("red %d %d ", cur_subphase, -offset);
                            state->state_time_speed.tab[index].timing.startTime = to_TimeMark(-offset);

                            offset = signal_status.StepSec;
                            offset += (signal_status.StepID <= 1 ? signal_status.plan[cur_subphase].PedGreenFlash : 0);
                            offset += (signal_status.StepID <= 2 ? signal_status.plan[cur_subphase].PedRed : 0);
                            offset += (signal_status.StepID <= 3 ? signal_status.plan[cur_subphase].Yellow : 0);
                            offset += (signal_status.StepID <= 4 ? signal_status.plan[cur_subphase].AllRed : 0);

                            increase_offset_red_signal();

                            SPaT_debug("%d\n", offset);
                            state->state_time_speed.tab[index].timing.minEndTime = to_TimeMark(offset);
                            state->state_time_speed.tab[index].timing_option = TRUE;
                            cur_subphase = next_subphase(cur_subphase);
                            SPaT_debug("green %d ", cur_subphase);
                            after_cur_step(greenType, offset += signal_status.plan[cur_subphase].Green, leading_subphase_signal(); lagging_subphase_signal(););
                            SPaT_debug("yellow %d ", cur_subphase);
                            after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].Yellow, );

                            signal_table[i] |= RedMask;  // 紅燈
                        }
                    } else {  // 行人綠
                        if (signal_status.phaseorder_plan[cur_subphase][i].SignalStatus & signal_mask_arr[j]) {
                            SPaT_debug("---++**\n");
                            int red_offset = signal_status.plan[cur_subphase].PedRed + signal_status.plan[cur_subphase].AllRed;
                            switch (signal_status.StepID) {
                            case 1:
                                offset = -(signal_status.plan[cur_subphase].PreGreen - signal_status.StepSec);
                                SPaT_debug("green %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_permissive_Movement_Allowed, offset = signal_status.StepSec, );
                                SPaT_debug("green flash %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].PedGreenFlash, );
                                SPaT_debug("red %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase].Yellow + red_offset, increase_offset_red_signal());
                                break;
                            case 2:
                                offset = -signal_status.plan[cur_subphase].PedGreenFlash - signal_status.StepSec;
                                SPaT_debug("green flash %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_protected_clearance, offset = signal_status.StepSec, );
                                SPaT_debug("red %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_stop_And_Remain, offset += signal_status.plan[cur_subphase].Yellow + red_offset, increase_offset_red_signal());
                                SPaT_debug("green %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_permissive_Movement_Allowed, offset += signal_status.plan[cur_subphase].PreGreen, );
                                break;
                            case 5:
                                offset += signal_status.plan[cur_subphase].AllRed;
                                red_offset = 0;
                            case 4:
                                offset += signal_status.plan[cur_subphase].Yellow;
                                red_offset -= signal_status.plan[cur_subphase].PedRed;
                            case 3:
                                offset += signal_status.plan[cur_subphase].PedRed;
                                offset = signal_status.StepSec - offset;
                                SPaT_debug("red %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_stop_And_Remain, offset = signal_status.StepSec + red_offset, increase_offset_red_signal());
                                SPaT_debug("green %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_permissive_Movement_Allowed, offset = signal_status.StepSec, );
                                SPaT_debug("green flash %d ", cur_subphase);
                                after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].PedGreenFlash, );
                            default:
                                break;
                            }
                        } else {  // 目前分相第一步接是紅燈
                            SPaT_debug("---**\n");
                            state->state_time_speed.count++;
                            state->state_time_speed.tab[index].eventState = MovementPhaseState_stop_And_Remain;

                            offset += signal_status.StepID >= 1 ? signal_status.plan[cur_subphase].PreGreen : 0;
                            offset += signal_status.StepID >= 2 ? signal_status.plan[cur_subphase].PedGreenFlash : 0;
                            offset += signal_status.StepID >= 3 ? signal_status.plan[cur_subphase].PedRed : 0;
                            offset += signal_status.StepID >= 4 ? signal_status.plan[cur_subphase].Yellow : 0;
                            offset += signal_status.StepID >= 5 ? signal_status.plan[cur_subphase].AllRed : 0;

                            offset -= signal_status.StepSec;
                            offset += signal_status.plan[cur_subphase].AllRed;
                            for (int k = (cur_subphase - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount;
                                 !(signal_status.phaseorder_plan[k][i].SignalStatus & signal_mask_arr[j]);
                                 k = (k - 1 + signal_status.SubPhaseCount) % signal_status.SubPhaseCount) {
                                offset += signal_status.plan[k].Green;
                                offset += signal_status.plan[k].Yellow;
                                offset += signal_status.plan[k].AllRed;
                            }
                            state->state_time_speed.tab[index].timing.startTime_option = TRUE;
                            SPaT_debug("red %d %d ", cur_subphase, -offset);
                            state->state_time_speed.tab[index].timing.startTime = to_TimeMark(-offset);

                            offset = signal_status.StepSec;
                            offset += (signal_status.StepID <= 1 ? signal_status.plan[cur_subphase].PedGreenFlash : 0);
                            offset += (signal_status.StepID <= 2 ? signal_status.plan[cur_subphase].PedRed : 0);
                            offset += (signal_status.StepID <= 3 ? signal_status.plan[cur_subphase].Yellow : 0);
                            offset += (signal_status.StepID <= 4 ? signal_status.plan[cur_subphase].AllRed : 0);

                            increase_offset_red_signal();

                            SPaT_debug("%d\n", offset);
                            state->state_time_speed.tab[index].timing.minEndTime = to_TimeMark(offset);
                            state->state_time_speed.tab[index].timing_option = TRUE;
                            cur_subphase = next_subphase(cur_subphase);
                            SPaT_debug("green %d ", cur_subphase);
                            after_cur_step(MovementPhaseState_permissive_Movement_Allowed, offset += signal_status.plan[cur_subphase].PreGreen, );
                            SPaT_debug("green flash %d ", cur_subphase);
                            after_cur_step(MovementPhaseState_protected_clearance, offset += signal_status.plan[cur_subphase].PedGreenFlash, );
                        }
                    }
                }
            }
        }
    }
    if (statesList->count == 0)
        return -1;

    // 如果有延長縮短指令放在 regional
    int_state->regional_option = FALSE;
    if (memcmp(signal_table, pre_signal_table, sizeof(signal_table)) == 0) {
        int tmp = get_adjust_time();
        adjust_time = tmp != 0 ? tmp : adjust_time;
        if (adjust_time != 0 && (adjust_flag || abs((signal_status.StepSec - preSec) > 3))) {
            adjust_flag = true;
            int_state->regional_option = TRUE;
            int_state->regional.count = 1;
            int_state->regional.tab = reg_spat;
            int_state->regional.tab[0].regionId = SPaT_Adjust_RegionalID;

            int tmp = htonl(adjust_time);
            asn1_ostr_clone_cstr(&int_state->regional.tab[0].u.unknown, (char *) &tmp, sizeof(tmp));
            LOG_MSG_TRACE("TIB adjust time %d", adjust_time);
        }
    } else {
        adjust_flag = adjust_time = 0;
        memcpy(pre_signal_table, signal_table, sizeof(signal_table));
    }
    preSec = signal_status.StepSec;

    return 1;
}
#undef to_TimeMark
#undef next_subphase
#undef after_cur_step
#undef increase_offset_red_signal
#undef leading_subphase_signal
#undef lagging_subphase_signal

void spat_printf(SPAT *pp_spat)
{
    IntersectionState *int_state = pp_spat->intersections.tab;
    LOG_MSG_TRACE("nowtime : %d", int_state->timeStamp);
    LOG_MSG_TRACE("moy : %d", int_state->moy);
    LOG_MSG_TRACE("region : %d", int_state->id.region);
    LOG_MSG_TRACE("id : %d", int_state->id.id);
    for (int i = 0; i < int_state->states.count; i++) {
        MovementState *state = &int_state->states.tab[i];
        LOG_MSG_TRACE("\nsignalGroup : %d %d", state->signalGroup, state->state_time_speed.count);

        for (int j = 0; j < state->state_time_speed.count; j++) {
            if (state->state_time_speed.tab[j].timing_option == TRUE) {
                if (state->state_time_speed.tab[j].eventState == MovementPhaseState_permissive_Movement_Allowed) {
                    LOG_MSG_TRACE("round green");
                } else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_protected_Movement_Allowed) {
                    LOG_MSG_TRACE("arrow green");
                } else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_protected_clearance) {
                    LOG_MSG_TRACE("yallow");
                } else if (state->state_time_speed.tab[j].eventState == MovementPhaseState_stop_And_Remain) {
                    LOG_MSG_TRACE("red");
                }
                LOG_MSG_TRACE(" startTime : %d ", state->state_time_speed.tab[j].timing.startTime);
                LOG_MSG_TRACE(" minEndTime : %d", state->state_time_speed.tab[j].timing.minEndTime);
            }
        }
    }
}