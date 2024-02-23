#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "EVSP.h"
#include "TSP.h"
#include "application_management_helper.h"
#include "application_registration.h"
#include "common_packet_tx.h"
#include "config.h"
#include "external_app_proxy_callback_msg_forward.h"
#include "external_app_proxy_server.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"

// todo: both above should be removed!
#define TIME_DEFENSE 5
#define gettid() syscall(__NR_gettid)

tsc_command_object_t command_buf[CYCLE_NUM][SUBPHASEID_NUM] = {0};
pthread_mutex_t mutex_command_buf = PTHREAD_MUTEX_INITIALIZER;

uint8_t cycle_index = 0;
uint8_t prior_cycle_index = 0;
uint8_t prior_SubPhaseID = 0;
uint8_t prior_StepID = 0;
uint16_t prior_StepSec = 0;

timer_t traffic_signal_command_buf_polling_timer_id;
uint8_t traffic_signal_command_buf_polling_num =
    TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING;

#define clear_index_command_buf(cyc_index, subphase_num) \
    memset(&command_buf[cyc_index][subphase_num], 0, sizeof(tsc_command_object_t))

// 由收到 5FCC 時更新避免 subphaseID 與 cycle index 不同步
// 沒有用 lock 因為在 5FCC 會修改
void update_cycle_index()
{
    cycle_index = (cycle_index + 1) % CYCLE_NUM;
}

void command_buf_init()
{
    int temp_ack_seq;
    sem_init(&sem_signal_status, 0, 1);
    // program begin enforce TC to pretime mode.
    temp_ack_seq = tsc_pretime();
    WAIT_ACK_LOOP
    /* command buffer polling timer event */
    create_timer(&traffic_signal_command_buf_polling_timer_id,
                 &traffic_signal_command_buf_polling_num, timer_event_handler);
    set_timer(traffic_signal_command_buf_polling_timer_id, 0, 500000000, 1,
              0);  // 0.5 msec start and interval is 1sec
}

void command_buf_clear()
{
    pthread_mutex_lock(&mutex_command_buf);
    memset(command_buf, 0, sizeof(command_buf));
    pthread_mutex_unlock(&mutex_command_buf);
    log_file_write("command buff is cleared\r\n");
}

void command_buf_delete_OBU(char host_OBU_name[ID_MAX_LEN + 1])
{
    int has_clean = false;
    pthread_mutex_lock(&mutex_command_buf);
    for (int i = 0; i < CYCLE_NUM; i++) {
        for (int j = 0; j < SUBPHASEID_NUM; j++) {
            if (strncmp(command_buf[i][j].host_OBU_name, host_OBU_name, ID_MAX_LEN + 1) == 0) {
                clear_index_command_buf(i, j);
                has_clean = true;
            }
        }
    }
    pthread_mutex_unlock(&mutex_command_buf);
    if (has_clean) {
        log_file_write("%-15s has been cleaned in command buffer.", host_OBU_name);
    }
}

void command_buf_search(int cycle, int subphaseID, tsc_command_object_t *command_obj)
{
    pthread_mutex_lock(&mutex_command_buf);
    memcpy(command_obj, &command_buf[(cycle_index + cycle) % CYCLE_NUM][subphaseID - 1], sizeof(tsc_command_object_t));
    pthread_mutex_unlock(&mutex_command_buf);
}

// 在切換日時段前 60 秒與後 10 分鐘停止控制
static inline void stop_at_segament_change(traffic_signal_status_t *signal_status, char log_content[LOG_CONTENT_LEN + 1])
{
    time_t currentTime;
    struct tm localTime;
    time(&currentTime);
    localtime_r(&currentTime, &localTime);
    for (int i = 0; i < signal_status->SegmentCount; i++) {
        uint8_t planHour = signal_status->allday_plan[i].Hour;
        uint8_t planMin = signal_status->allday_plan[i].Min;

        int timeDiff = (((planHour - localTime.tm_hour) * 60 + (planMin - localTime.tm_min)) * 60) - localTime.tm_sec;
        if (-600 <= timeDiff && timeDiff <= 60) {
            tsc_pretime();
            command_buf_clear();
            compensation_buffer_clear();
            log_snprintf(log_content, "Enforce to pretime control_strategy\r\n");
            break;
        }
    }
}

// 要送command到tc箱 被polling呼叫
void command_buf_send(tsc_command_object_t *command_obj, uint8_t current_SubPhaseID)
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    sem_timedwait_millsecs(
        &sem_signal_status,
        SEM_SIGNAL_STATUS_TIMEOUT);  // 有timeout的號誌等待 但是 對應的post在那？

    uint16_t pretime = signal_status.plan[current_SubPhaseID - 1].PreGreen;
    int difference = 0;
    int time = 0;
    int temp_ack_seq;

    uint8_t conpensation_flag = false;
    conpensation_flag = is_in_compensation();
    uint16_t current_sec_residual = signal_status.StepSec;

    if (command_obj->app_id == TSP.id) {  // 這裡就算要核對app_id也應該要從app_list裡面去撈 而不是這樣直接assign!!
        if (TSP.dontSend2TC == 1) {
            printf("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
            log_file_write("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
            return;
        }
        if (conpensation_flag) {
            printf("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
            log_file_write("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
            return;
        }
    } else if (command_obj->app_id == EVSP.id) {
        if (EVSP.dontSend2TC == 1) {
            log_file_write("EVSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
            return;
        }
    } else {
        log_file_write("not TSP either EVSP is sent to TC machine\r\n");
    }

    if (config.log_command_buffer) {
        log_file_write("command_buf_send: \neffect time: %d", command_obj->effect_time);
    }

    switch (config.signal_controller_manufacturer) {
    case CHENG_LONG:
        // command_obj->adjusted_time代表這個step現在的時間
        difference = command_obj->effect_time - command_obj->adjusted_time;
        // printf("cmd obj's effect time is %d and adjusted time is %d\r\n", command_obj->effect_time, command_obj->adjusted_time);
        // below to prevent cheng_long 655xx error
        int16_t original_difference = 0;
        original_difference = difference;

        if (((int) current_sec_residual + difference) < TIME_DEFENSE && current_sec_residual >= TIME_DEFENSE) {
            difference = TIME_DEFENSE - current_sec_residual;  // 能夠忍受的砍的值
            printf("difference has been changed from %d to %d(cheng_long)\r\n", original_difference, difference);
            log_file_write("\ndifference has been changed from %d to %d(cheng_long)", original_difference, difference);
        }

        // 晟隆需要跟此步階下原本定時制下計劃的秒數（PreTimeCompensated）比較
        time = pretime + difference;  // difference才是真正會延長的時間
        // printf("diff: %d, time: %d, pretime: %d\n", difference, time,
        // pretime);
        if (time > 255) {
            time = 255;
        }

        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP

        while (time < 0) {
            // 不能下0 否則step會立刻結束
            temp_ack_seq = tsc_extend(current_SubPhaseID, 1, 1);  // 每次就是pretime-4去扣
            WAIT_ACK_LOOP
            // time += pretime;
            time += (pretime - 4);  // 要想一下 -4是因為機器限制的關係
        }
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;

    case SHAN_ZHU:
        difference = command_obj->effect_time - command_obj->adjusted_time;
        log_file_write("\ndifference is :%d\r\n", difference);

        time = command_obj->effect_time;
        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;

    case SHAN_ZHU_M:
        difference = command_obj->effect_time - command_obj->adjusted_time;
        printf("difference:%d\r\n", difference);
        log_file_write("\ndifference is :%d\r\n", difference);

        time = command_obj->effect_time;
        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;
    default:
        return;  // 不屬於任何一家號控器
        break;
    }
    // return值為5fcc
    temp_ack_seq = tsc_5F4C();  // query的輸出會在上面log evsp/tsp enable/disable的上方
    WAIT_ACK_LOOP
    if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME)) != 0) {
        set_compensation_buffer(current_SubPhaseID);
    }

    /* traffic signal command tx event */
    traffic_signal_command_arg_t command;  // this variable is for callback of signal packet tx
    command.control_status = command_obj->app_id;
    command.phase = current_SubPhaseID;
    command.step = 1;
    command.effect_time = command_obj->effect_time;
    memcpy(command.host_OBU_name, command_obj->host_OBU_name, sizeof(command.host_OBU_name));

    /* since now dispatcher, ea_app_proxy, command_buf_send(),
     * all might read/write callback_list, we add a mutex_lock */
    // pthread_mutex_lock(&mutex_callback_list);

    // 執行callback 完全不管 app_id 了 event signal packet tx
    // goto TSP_report_command()
    event_callback_t *current = &callback_list[EVENT_TRAFFIC_SIGNAL_COMMAND_TX];
    while (current->next != NULL) {
        proxy_handling_app_p = current->next->app_obj_p;
        current->next->callback((void *) &command);
        current = current->next;
    }

    // pthread_mutex_unlock(&mutex_callback_list);
}

// In order to enable the commands in the commmand buffer to be sent to the
// traffic signal controller at an appropriate time.

static inline bool check_command_buf_empty()
{
    for (int i = 0; i < CYCLE_NUM; i++) {
        for (int j = 0; j < SUBPHASEID_NUM; j++) {
            if (command_buf[i][j].app_id != 0) {
                pthread_mutex_unlock(&mutex_command_buf);
                return false;
            }
        }
    }
    return true;
}

void command_buf_polling()
{
    char log_content[LOG_CONTENT_LEN + 1] = {0};

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        goto POLLING_END;
    }

    uint8_t current_SubPhaseID = signal_status.SubPhaseID;
    uint8_t current_StepID = signal_status.StepID;
    uint16_t current_StepSec = signal_status.StepSec;

    if (config.log_command_buffer) {
        log_snprintf(log_content,
                     "prior signal status: SubPhaseID(%d) StepID(%d) StepSec(%d)"
                     "\ncurrent signal status: SubPhaseID(%d) StepID(%d) StepSec(%d)\n",
                     prior_SubPhaseID, prior_StepID, prior_StepSec,  // here all are global variables
                     current_SubPhaseID, current_StepID, current_StepSec);
    }

    if (prior_SubPhaseID == 0 || current_SubPhaseID == 0) {
        prior_SubPhaseID = current_SubPhaseID;
        prior_StepID = current_StepID;
        prior_StepSec = current_StepSec;
        goto POLLING_END;
    }

    pthread_mutex_lock(&mutex_command_buf);
    /* cross to next phase */

    /* clear last command buf */
    // This means that traffic signal has crossed to the next subphase.
    // Thus,the command buffer object of the previous subphase is cleared.
    if (prior_SubPhaseID != current_SubPhaseID) {
        // 當發現前一個指令是 resume 就進行補償
        if (strncmp(command_buf[prior_cycle_index][prior_SubPhaseID - 1].host_OBU_name,
                    RESUME_ID, sizeof(RESUME_ID) - 1) == 0) {
            pthread_mutex_unlock(&mutex_command_buf);
            start_compensation();  // 開始進行補償
            pthread_mutex_lock(&mutex_command_buf);
        }
        clear_index_command_buf(prior_cycle_index, prior_SubPhaseID - 1);
        // 設定為 0 代表現在沒有 app 控制
        set_control_status(0);
        // 換相了 更新adjusted time讓他變成現在的倒數秒數
        command_buf[cycle_index][current_SubPhaseID - 1].adjusted_time = current_StepSec;
    }
    // execute pretime instruction to force tc go back to pretime
    // to prevent the tc not go back to pretime after 全動態
    // pretime_sent_count is for let pretime sent one time only in step 4
    // now, have to check command buffer whether or not is empty
    // if it is empty and return pretime control status.
    static uint8_t pretimeflag = true;
    if (signal_status.StepID == 4) {
        if (check_command_buf_empty() && pretimeflag) {
            pretimeflag = false;
            uint8_t temp_ack_seq = tsc_pretime();
            WAIT_ACK_LOOP
        }
    } else {
        pretimeflag = true;
    }
    /* command ready to send in current phase */
    // app_id != 0 代表有指令
    if (command_buf[cycle_index][current_SubPhaseID - 1].app_id != 0 &&
        command_buf[cycle_index][current_SubPhaseID - 1].send_flag == false &&
        current_StepID == 1 && current_StepSec > 1 && prior_SubPhaseID == current_SubPhaseID) {
        // 將目前phase的 command buffer object 送到TC箱
        command_buf_send(&command_buf[cycle_index][current_SubPhaseID - 1], current_SubPhaseID);
        log_snprintf(log_content, "command_buf_send(command_buf[%d][%d])\r\n", cycle_index, current_SubPhaseID);
        set_control_status(command_buf[cycle_index][current_SubPhaseID - 1].app_id);  // 判斷是evsp還是tsp
        command_buf[cycle_index][current_SubPhaseID - 1].send_flag = true;            // 已送出TC箱
        // 更新步階一要倒數的時間
        command_buf[cycle_index][current_SubPhaseID - 1].adjusted_time =
            command_buf[cycle_index][current_SubPhaseID - 1].effect_time;
    }
    prior_cycle_index = cycle_index;
    prior_SubPhaseID = current_SubPhaseID;
    prior_StepID = current_StepID;
    prior_StepSec = current_StepSec;
    pthread_mutex_unlock(&mutex_command_buf);

    log_snprintf(log_content, "tsp and evsp status now:\r\n1.dont send to TSP:%d\n\r2.dont send to EVSP:%d\r\n",
                 TSP.dontSend2TC, EVSP.dontSend2TC);
POLLING_END:
    command_buf_print();
    log_file_write(log_content);
}

// 被command_buf_insert_adjustment和evsp呼叫
int command_buf_insert_effect_time(tsc_command_t *command)
{
    /* command value valid */
    if (command->app_id == 0) {
        return INVALID_APP_ID;
    }
    if (command->app_priority == 0) {
        return INVALID_APP_PRIORITY;
    }
    if (command->target_phase == 0 || command->target_phase > SUBPHASEID_NUM) {
        return INVALID_TARGET_PHASE;
    }
    if (command->cycle >= CYCLE_NUM) {
        return INVALID_CYCLE;
    }
    if (command->phase == 0 || command->phase > SUBPHASEID_NUM) {
        return INVALID_PHASE;
    }
    if (command->effect_time < 0) {
        return INVALID_EFFECT_TIME;
    }
    if (strlen(command->host_OBU_name) == 0) {
        return INVALID_HOST_OBU_NAME;
    }

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    uint16_t pretime = signal_status.plan[command->phase - 1].PreTimeCompensated;
    uint16_t min_green = signal_status.plan[command->phase - 1].MinGreen;
    uint16_t max_green = signal_status.plan[command->phase - 1].MaxGreen;
    uint16_t PedGreen = signal_status.plan[command->phase - 1].PedGreenFlash + signal_status.plan[command->phase - 1].PedRed;

    if (command->effect_time + PedGreen < min_green) {
        command->effect_time = min_green - PedGreen;
    }
    if (command->effect_time + PedGreen > max_green) {
        command->effect_time = max_green - PedGreen;
    }

    pthread_mutex_lock(&mutex_command_buf);
    // 抓出要處理的cmd buff object
    //  comman->cycle and phase are used to "indicate the index of the target_command buffer object".
    tsc_command_object_t *target_command_obj = &command_buf[(cycle_index + command->cycle) % CYCLE_NUM][command->phase - 1];

    // command object first insert
    if (target_command_obj->app_id == 0) {
        // adjusted_time is the length of time that the "traffic signal controller" is adjusted to.
        if (target_command_obj->adjusted_time == 0) {  // 因為此步階預設倒數時間為 pretime
            target_command_obj->adjusted_time = pretime;
        }
        goto COMMAND_BUF_INSERT_ACCEPT_APP_ID;
    }

    // 補償指令要可以被下一個補償指令覆蓋 因為下一個近來的補償會考慮之後的延長
    if (strncmp(target_command_obj->host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME)) == 0 &&
        strncmp(command->host_OBU_name, COMPENSATION_NAME, sizeof(COMPENSATION_NAME)) == 0) {
        goto COMMAND_BUF_INSERT_ACCEPT_EFFECT_TIME;
    }

    // resume 是 app 要回復原本時治狀態下的指令 所以可以被取代
    // 因為 host_OBU_name 有可能是 RESUME_ID_DONE 所以 sizeof(RESUME_ID) - 1
    if (strncmp(target_command_obj->host_OBU_name, RESUME_ID, sizeof(RESUME_ID)) == 0) {
        goto COMMAND_BUF_INSERT_ACCEPT_APP_ID;
    }

    // resume command
    if (strncmp(command->host_OBU_name, RESUME_ID, sizeof(RESUME_ID)) == 0) {
        if (target_command_obj->app_id == command->app_id) {
            // 還是會使用 special_OBU_list_update_status
            // 但是會在 OBU_object_search 的時候沒有找到
            goto COMMAND_BUF_INSERT_ACCEPT_OBU_NAME;
        } else {
            goto COMMAND_BUF_IMPROPER_PRIORITY;
        }
    }

    // same OBU ID      appid的check看起來像是多餘的
    // 除非是一個obu有多個application This means that the application has made a
    // "new command for the serviced OBU". Replace the original command
    if (strncmp(target_command_obj->host_OBU_name, command->host_OBU_name, OBU_NAME_MAX_LEN) == 0 &&
        target_command_obj->app_id == command->app_id) {
        goto COMMAND_BUF_INSERT_ACCEPT_EFFECT_TIME;
    }

    // same target phase
    // Consider the effect_time of two commands and attempt to determine which
    // command can "serve two OBUs at the same time".
    if (target_command_obj->target_phase == command->target_phase) {
        // time difference between effect time & pretime increase
        // 可以延長不能縮短
        if (abs(target_command_obj->effect_time - pretime) <= abs(command->effect_time - pretime)) {
            goto COMMAND_BUF_INSERT_ACCEPT_APP_ID;
        } else {
            goto COMMAND_BUF_IMPROPER_PRIORITY;
        }
    } else {
        // TSP的target_phase不一定是目前的phase
        // 但EVSP的target_phase是目前的phase
        // 所以要判斷優先權大小來取代
        // different target phase
        // priority higher than original command 數值越小priority越高
        if (target_command_obj->app_priority > command->app_priority) {
            goto COMMAND_BUF_INSERT_ACCEPT_APP_ID;
        } else {
            goto COMMAND_BUF_IMPROPER_PRIORITY;
        }
    }

COMMAND_BUF_INSERT_ACCEPT_APP_ID:
    target_command_obj->app_id = command->app_id;
    target_command_obj->app_priority = command->app_priority;
COMMAND_BUF_INSERT_ACCEPT_OBU_NAME:
    strncpy(target_command_obj->host_OBU_name, command->host_OBU_name, sizeof(command->host_OBU_name));
    target_command_obj->host_OBU_name[sizeof(command->host_OBU_name) - 1] = '\0';
COMMAND_BUF_INSERT_ACCEPT_EFFECT_TIME:
    target_command_obj->effect_time = command->effect_time;
    target_command_obj->target_phase = command->target_phase;
    target_command_obj->send_flag = false;
COMMAND_BUF_INSERT_ACCEPT:
    pthread_mutex_unlock(&mutex_command_buf);
    command_buf_print();
    return INSERT_ACCEPT;
COMMAND_BUF_IMPROPER_PRIORITY:
    pthread_mutex_unlock(&mutex_command_buf);
    command_buf_print();
    return IMPROPER_PRIORITY;
}

// 調整要送到command_buf_insert_effect_time的command結構的值
int command_buf_insert_adjustment(tsc_command_t *command)
{
    uint8_t is_in_compensation_flag = is_in_compensation();
    if (is_in_compensation_flag == true)
        return INCOMP_TSPDONOTHING;

    /* command value valid */
    if (command->adjustment == 0) {
        return INVALID_ADJUSTMENT;
    }

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    uint16_t pretime = signal_status.plan[command->phase - 1].PreTimeCompensated;
    uint16_t min_green = signal_status.plan[command->phase - 1].MinGreen;
    uint16_t max_green = signal_status.plan[command->phase - 1].MaxGreen;

    if (config.signal_adjust_lower_bound_active) {
        int16_t lower = pretime - (pretime * config.signal_adjust_lower_bound_percentage * 0.01);
        min_green = (lower > min_green) ? lower : min_green;
    }

    if (config.signal_adjust_upper_bound_active) {
        int16_t upper = pretime + (pretime * config.signal_adjust_upper_bound_percentage * 0.01);
        max_green = (upper < max_green) ? upper : max_green;
    }

    pthread_mutex_lock(&mutex_command_buf);
    // 抓出target phase的原始資料
    tsc_command_object_t *target_command_obj = &command_buf[(cycle_index + command->cycle) % CYCLE_NUM][command->phase - 1];

    // This means that this subphase has never been adjusted
    if (target_command_obj->adjusted_time == 0) {  // 第一次被調整？
        command->effect_time = pretime + command->adjustment;
    } else {
        command->effect_time = target_command_obj->adjusted_time + command->adjustment;
    }
    pthread_mutex_unlock(&mutex_command_buf);

    if (command->effect_time < min_green) {
        command->effect_time = min_green;
    }

    if (command->effect_time > max_green) {
        command->effect_time = max_green;
    }

    if (config.log_command_buffer) {
        log_file_write("command_buf_insert_adjustment: \nOBU ID: %s\nadjustment:  %d\neffect time: %d",
                       command->host_OBU_name, command->adjustment, command->effect_time);
    }
    return command_buf_insert_effect_time(command);
}

// APP 結束會恢復狀態 會刪除剩餘的 host OBU command 並回復 current phase 原本的路燈時間
// 並在下一個 phase 會開始補償
// 如果 current phase 已經不是 step 1 也會插入 因為下一個 phase 的補償會幫助
int command_buf_resume_control(uint8_t appid)
{
    tsc_command_t command = {0};
    traffic_signal_status_t signal_status;
    app_obj_t *app_obj = get_app_obj_by_appID(appid);
    if (app_obj == NULL || app_obj->dontSend2TC == 1)
        return -1;
    get_traffic_signal_status(&signal_status);

    command.app_id = 99;
    command.app_priority = 99;
    command.target_phase = signal_status.SubPhaseID;
    strncpy(command.host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN);
    command.phase = command.target_phase;
    command.effect_time = signal_status.plan[command.target_phase - 1].PreGreen;

    log_file_write("app id %d insert resume.", appid);
    return command_buf_insert_effect_time(&command);
}

void command_buf_print()
{  // 就要不要log command buffer的開關
    if (config.log_command_buffer == 0) {
        return;
    }
    traffic_signal_status_t signal_status;
    char log_content[LOG_CONTENT_LEN + 1];

    get_traffic_signal_status(&signal_status);
    memset(log_content, 0, sizeof(log_content));

    pthread_mutex_lock(&mutex_command_buf);
    log_snprintf(log_content, "command buffer: current cycle index (%d) current SubPhaseID (%d)",
                 cycle_index, signal_status.SubPhaseID);
    for (int i = 0; i < CYCLE_NUM; i++) {
        for (int j = 0; j < signal_status.SubPhaseCount; j++) {
            log_snprintf(log_content,
                         "\ncmd[%d][%d]: AT:%3d, PT:%3d, HoID:%-15s, TP:%1d, AppID:%2d, AppPri:%2d, ET:%3d, SF:%1d",
                         i, j + 1, command_buf[i][j].adjusted_time,
                         signal_status.plan[j].PreGreen, command_buf[i][j].host_OBU_name,
                         command_buf[i][j].target_phase, command_buf[i][j].app_id,
                         command_buf[i][j].app_priority, command_buf[i][j].effect_time,
                         command_buf[i][j].send_flag);
        }
    }
    pthread_mutex_unlock(&mutex_command_buf);
    log_file_write(log_content);
}