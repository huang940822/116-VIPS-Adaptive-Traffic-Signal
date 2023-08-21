#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "EVSP.h"
#include "TSP.h"
#include "application_registration.h"
#include "common_packet_tx.h"
#include "config.h"
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

tsc_command_object_t command_buf[CYCLE_NUM][SUBPHASEID_NUM];
pthread_mutex_t mutex_command_buf = PTHREAD_MUTEX_INITIALIZER;

uint8_t cycle_index = 0;
uint8_t prior_SubPhaseID = 0;
uint8_t prior_StepID = 0;
uint16_t prior_StepSec = 0;

timer_t traffic_signal_command_buf_polling_timer_id;
uint8_t traffic_signal_command_buf_polling_num =
    TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING;
static uint8_t CompensationInitialFlag = true;
static uint8_t CompensationFlag = false;
static uint8_t count = 0;
// extern pthread_mutex_t mutex_rs232_write;
// extern int16_t ack_seq;


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

// 要送command到tc箱 被polling呼叫
void command_buf_send(tsc_command_object_t *command_obj,
                      uint8_t current_SubPhaseID)
{
    if (config.log_command_buffer) {
        log_file_write("command_buf_send: \neffect time: %d", command_obj->effect_time);
    }

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);
    //????等待某些東西？
    sem_timedwait_millsecs(
        &sem_signal_status,
        SEM_SIGNAL_STATUS_TIMEOUT);  // 有timeout的號誌等待 但是 對應的post在那？

    uint16_t pretime =
        signal_status.plan[current_SubPhaseID - 1].PreTimeCompensated;
    int difference = 0;
    int time = 0;
    int temp_ack_seq;

    uint8_t conpensation_flag = false;
    conpensation_flag = is_in_compensation();
    // 公車來臨若TC正在補償則不做控制

    // EVSP.dontSend2TC = 0;
    uint16_t current_sec_residual = get_current_second();

    /* 未來如果要去 撈 app_list, 記得前後要包 mutex_app_list */
    switch (config.signal_controller_manufacturer) {
    case CHENG_LONG:
        if (command_obj->app_id == TSP.id) {  // 這裡就算要核對app_id也應該要從app_list裡面去撈 而不是這樣直接assign!!
            if (TSP.dontSend2TC == 1) {
                printf("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
            if (conpensation_flag) {
                printf("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                break;
            }

        } else if (command_obj->app_id == EVSP.id) {
            if (EVSP.dontSend2TC == 1) {
                log_file_write("EVSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
        } else {
            log_file_write("not TSP either EVSP is sent to TC machine\r\n");
        }

        // command_obj->adjusted_time代表這個step現在的時間
        difference = command_obj->effect_time - command_obj->adjusted_time;
        // printf("cmd obj's effect time is %d and adjusted time is %d\r\n", command_obj->effect_time, command_obj->adjusted_time);


        // below to prevent cheng_long 655xx error
        int16_t original_difference = 0;
        original_difference = difference;
        char log_content[LOG_CONTENT_LEN + 1];
        memset(log_content, 0, sizeof(log_content));


        if (((int) current_sec_residual + difference) < TIME_DEFENSE && current_sec_residual >= TIME_DEFENSE) {
            difference = TIME_DEFENSE - current_sec_residual;  // 能夠忍受的砍的值
            printf("difference has been changed from %d to %d(cheng_long)\r\n", original_difference, difference);
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\ndifference has been changed from %d to %d(cheng_long)",
                     original_difference, difference);
            log_file_write(log_content);
        }

        // compensation_buffer_initialization
        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, 15) != 0) {
            if (CompensationInitialFlag == true) {
                get_compensation_buffer(compensation_buffer);
                CompensationInitialFlag = false;
            }
        }

        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, COMPENSATION_LEN) != 0) {
            if (current_sec_residual + difference < 0) {
                int16_t residual = difference + current_sec_residual;
                printf("residual:%d\r\n", residual);
                compensation_buffer[current_SubPhaseID - 1] += (difference - residual);
            } else {
                compensation_buffer[current_SubPhaseID - 1] += difference;
            }
        }

        // 晟隆需要跟此步階下原本定時制下計劃的秒數（PreTimeCompensated）比較
        time = pretime + difference;  // difference才是真正會延長的時間
        // printf("diff: %d, time: %d, pretime: %d\n", difference, time,
        // pretime);
        if (time > 255) {
            time = 255;
        }

        while (time < 0) {
            temp_ack_seq = tsc_dynamic();
            WAIT_ACK_LOOP
            // 不能下0 否則step會立刻結束
            temp_ack_seq =
                tsc_extend(current_SubPhaseID, 1, 1);  // 每次就是pretime-4去扣
            WAIT_ACK_LOOP
            // time += pretime;
            time += (pretime - 4);  // 要想一下 -4是因為機器限制的關係
        }

        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;

    case SHAN_ZHU:
        if (command_obj->app_id == TSP.id) {  // 這裡就算要核對app_id也應該要從app_list裡面去撈 而不是這樣直接assign!!
            if (TSP.dontSend2TC == 1) {
                printf("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
            if (conpensation_flag) {
                printf("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                break;
            }

        } else if (command_obj->app_id == EVSP.id) {
            if (EVSP.dontSend2TC == 1) {
                log_file_write("EVSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
        } else {
            log_file_write("not TSP either EVSP is sent to TC machine\r\n");
        }

        difference = command_obj->effect_time - command_obj->adjusted_time;
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ndifference is :%d\r\n", difference);
        log_file_write(log_content);

        // compensation_buffer_initialization
        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, 15) != 0) {
            if (CompensationInitialFlag == true) {
                get_compensation_buffer(compensation_buffer);
                CompensationInitialFlag = false;
            }
        }

        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, 15) != 0) {
            if (current_sec_residual + difference < 0) {
                int16_t residual = difference + current_sec_residual;
                printf("residual:%d\r\n", residual);
                compensation_buffer[current_SubPhaseID - 1] += (difference - residual);
            } else {
                compensation_buffer[current_SubPhaseID - 1] += difference;
            }
        }

        time = command_obj->effect_time;
        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;

    case SHAN_ZHU_M:
        if (command_obj->app_id == TSP.id) {  // 這裡就算要核對app_id也應該要從app_list裡面去撈 而不是這樣直接assign!!
            if (TSP.dontSend2TC == 1) {
                printf("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
            if (conpensation_flag) {
                printf("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                log_file_write("TSP cmd isn't sent to TC machine ,for conpensation_flag enabled\r\n");
                break;
            }
        } else if (command_obj->app_id == EVSP.id) {
            if (EVSP.dontSend2TC == 1) {
                log_file_write("EVSP cmd isn't sent to TC machine for dontSend2TC enabled\r\n");
                break;
            }
        } else {
            log_file_write("not TSP either EVSP is sent to TC machine\r\n");
        }

        difference = command_obj->effect_time - command_obj->adjusted_time;
        printf("difference:%d\r\n", difference);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\ndifference is :%d\r\n", difference);
        log_file_write(log_content);

        // compensation_buffer_initialization
        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, 15) != 0) {
            if (CompensationInitialFlag == true) {
                get_compensation_buffer(compensation_buffer);
                CompensationInitialFlag = false;
            }
        }

        if (strncmp(command_obj->host_OBU_name, COMPENSATION_NAME, COMPENSATION_LEN) != 0) {
            if (current_sec_residual + difference < 0) {
                int16_t residual = difference + current_sec_residual;
                printf("residual:%d\r\n", residual);
                compensation_buffer[current_SubPhaseID - 1] += (difference - residual);
            } else {
                compensation_buffer[current_SubPhaseID - 1] += difference;
            }
        }
        time = command_obj->effect_time;
        temp_ack_seq = tsc_dynamic();
        WAIT_ACK_LOOP
        temp_ack_seq = tsc_extend(current_SubPhaseID, 1, time);
        WAIT_ACK_LOOP
        break;
    default:
        break;
    }
    // return值為5fcc
    // pthread_mutex_lock(&mutex_rs232_write);
    temp_ack_seq =
        tsc_5F4C();  // query的輸出會在上面log evsp/tsp enable/disable的上方
    WAIT_ACK_LOOP
    // pthread_mutex_unlock(&mutex_rs232_write);

    /* traffic signal command tx event */
    traffic_signal_command_arg_t
        command;  // this variable is for callback of signal packet tx
    command.control_status = command_obj->app_id;
    command.phase = current_SubPhaseID;
    command.step = 1;
    command.effect_time = command_obj->effect_time;
    memcpy(command.host_OBU_name, command_obj->host_OBU_name, OBU_NAME_MAX_LEN + 1);

    /* since now dispatcher, ea_app_proxy, command_buf_send(), 
    * all might read/write callback_list, we add a mutex_lock */
    pthread_mutex_lock(&mutex_callback_list); 

    // 執行callback 完全不管 app_id 了 event signal packet tx
    // goto TSP_report_command()
    event_callback_t *current = &callback_list[EVENT_TRAFFIC_SIGNAL_COMMAND_TX];
    while (current->next != NULL) {
        current->next->callback((void *) &command);
        current = current->next;
    }

    pthread_mutex_unlock(&mutex_callback_list); 
}
// In order to enable the commands in the commmand buffer to be sent to the
// traffic signal controller at an appropriate time.
void command_buf_polling()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    traffic_signal_status_t signal_status;

    // get the status of when??? now?
    get_traffic_signal_status(&signal_status);
    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        command_buf_print();
        return;
    }

    uint8_t current_SubPhaseID = signal_status.SubPhaseID;
    uint8_t current_StepID = signal_status.StepID;
    uint16_t current_StepSec = signal_status.StepSec;

    if (config.log_command_buffer) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "command_buf_polling: ");

        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\n%-23sSubPhaseID(%d) StepID(%d) StepSec(%d)",
                 "prior signal status:", prior_SubPhaseID, prior_StepID,
                 prior_StepSec);  // here all are global variables

        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\n%-23sSubPhaseID(%d) StepID(%d) StepSec(%d)",
                 "current signal status:", current_SubPhaseID, current_StepID,
                 current_StepSec);

        log_file_write(log_content);
    }
    memset(log_content, 0, sizeof(log_content));

    // 何時phase會是0 人為設定的？？
    if (prior_SubPhaseID == 0 || current_SubPhaseID == 0) {
        prior_SubPhaseID = current_SubPhaseID;
        prior_StepID = current_StepID;
        prior_StepSec = current_StepSec;
        command_buf_print();
        return;
    }
    pthread_mutex_lock(&mutex_command_buf);
    /* cross to next phase */

    /* clear last command buf */
    // This means that traffic signal has crossed to the next subphase.
    // Thus,the command buffer object of the previous subphase is cleared.
    if (prior_SubPhaseID != current_SubPhaseID) {
        if (strncmp(command_buf[cycle_index][prior_SubPhaseID - 1].host_OBU_name,
                    COMPENSATION_NAME, COMPENSATION_LEN) == 0) {
            if (command_buf[cycle_index][prior_SubPhaseID - 1].send_flag ==
                true) {
                snprintf(
                    log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "\ncommand_buf[%d][%d]: HoID:%-15s is cleared\r\n",
                    cycle_index, prior_SubPhaseID,
                    command_buf[cycle_index][prior_SubPhaseID - 1].host_OBU_name);
                log_file_write(log_content);
                memset(&command_buf[cycle_index][prior_SubPhaseID - 1], 0,
                       sizeof(tsc_command_object_t));
            }
        } else {
            memset(&command_buf[cycle_index][prior_SubPhaseID - 1], 0,
                   sizeof(tsc_command_object_t));
        }
        // 設定為0代表不控制？
        set_control_status(0);
    }
    /* cross to next cycle */
    // 代表已經到下一個cycle.
    // 所以要把previous cycle 中的command buffer object 清除
    // compensation_command不能清除，除非他已經送出了。
    if (prior_SubPhaseID > current_SubPhaseID) {
        for (int i = 0; i < SUBPHASEID_NUM; i++) {
            if (strncmp(command_buf[cycle_index][i].host_OBU_name,
                        COMPENSATION_NAME, COMPENSATION_LEN) == 0) {
                if (command_buf[cycle_index][i].send_flag == true) {
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "\rcommand_buf[%d][%d]: HoID:%-15s is cleared\r\n",
                             cycle_index, prior_SubPhaseID,
                             command_buf[cycle_index][prior_SubPhaseID - 1]
                                 .host_OBU_name);
                    log_file_write(log_content);
                    memset(&command_buf[cycle_index][i], 0, sizeof(tsc_command_object_t));
                } else
                    continue;
            } else {
                snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "\rcommand_buf[%d][%d]: HoID:%-15s is cleared\r\n",
                             cycle_index, prior_SubPhaseID,
                             command_buf[cycle_index][prior_SubPhaseID - 1].host_OBU_name);
                log_file_write(log_content);
                memset(&command_buf[cycle_index][i], 0, sizeof(tsc_command_object_t));
            }
        }
        // memset(&command_buf[cycle_index][0], 0, sizeof(tsc_command_object_t)
        // * SUBPHASEID_NUM);
        cycle_index = (cycle_index + 1) % CYCLE_NUM;  // 更新cycle
    }

    if (command_buf[cycle_index][current_SubPhaseID - 1].adjusted_time == 0 &&
        prior_SubPhaseID != current_SubPhaseID) {
        command_buf[cycle_index][current_SubPhaseID - 1].adjusted_time =
            current_StepSec;  // 換相了 更新adjusted time讓他變成現在的倒數秒數
    }

    uint8_t compensation_send_flag = true;

    /* command ready to send in current phase */
    if (command_buf[cycle_index][current_SubPhaseID - 1].send_flag == false &&
        command_buf[cycle_index][current_SubPhaseID - 1].app_id != 0 &&
        current_StepID == 1 && current_StepSec > 1 &&
        prior_SubPhaseID == current_SubPhaseID) {
        if (strncmp(command_buf[cycle_index][current_SubPhaseID - 1].host_OBU_name, COMPENSATION_NAME, COMPENSATION_LEN) == 0) {
            int16_t residual_time = current_StepSec - command_buf[cycle_index][current_SubPhaseID - 1].compensation_time;
            printf("residual_time:%d\r\n", residual_time);
            if (residual_time <= TIME_DEFENSE) {
                compensation_send_flag = false;
                printf("***");
            }
        }
        if (compensation_send_flag == true) {
            // 將目前phase的 command buffer object 送到TC箱
            command_buf_send(&command_buf[cycle_index][current_SubPhaseID - 1], current_SubPhaseID);
            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "command_buf_send(command_buf[%d][%d])\r\n", cycle_index, current_SubPhaseID);
            log_file_write(log_content);
            set_control_status(command_buf[cycle_index][current_SubPhaseID - 1].app_id);  // 判斷是evsp還是tsp
            command_buf[cycle_index][current_SubPhaseID - 1].send_flag = true;            // 已送出TC箱
            CompensationFlag = true;
            // 更新步階一要倒數的時間
            command_buf[cycle_index][current_SubPhaseID - 1].adjusted_time =
                command_buf[cycle_index][current_SubPhaseID - 1].effect_time;
        }
    }

    prior_SubPhaseID = current_SubPhaseID;
    prior_StepID = current_StepID;
    prior_StepSec = current_StepSec;
    command_buf_print();
    pthread_mutex_unlock(&mutex_command_buf);

    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "tsp and evsp status now:\r\n1.dont send to TSP:%d\n\r2.dont send "
             "to EVSP:%d",
             TSP.dontSend2TC, EVSP.dontSend2TC);
    log_file_write(log_content);


    // only EVSP control instruction will goto `if`
    // ensure compensation buffer will be up-to-date after RESUME instruction
    // execute.
    if (strncmp(command_buf[cycle_index][current_SubPhaseID - 1].host_OBU_name,
                RESUME_ID, OBU_NAME_MAX_LEN) == 0) {
        if (command_buf[cycle_index][current_SubPhaseID - 1].send_flag ==
                true &&
            CompensationFlag) {
            log_file_write("RESUME instruction is executed\r\n");
            printf("RESUME instruction is executed.\r\n");

            report_compensation_time();

            switch (config.traffic_compensation_method) {
            case 1:
                traffic_compensation_method1(config.traffic_compensation_cycle_number);
                break;
            case 2:
                traffic_compensation_method2(config.traffic_compensation_cycle_number, config.phase_weight);
                break;
            case 3:
                traffic_compensation_method3(config.traffic_compensation_cycle_number);
                break;
            default:
                break;
            }
            CompensationFlag = false;
        }
    }
    return;
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
    if (command->effect_time <= 0) {
        return INVALID_EFFECT_TIME;
    }
    if (strlen(command->host_OBU_name) == 0) {
        return INVALID_HOST_OBU_NAME;
    }

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    uint16_t pretime =
        signal_status.plan[command->phase - 1].PreTimeCompensated;
    uint16_t min_green = signal_status.plan[command->phase - 1].MinGreen;
    uint16_t max_green = signal_status.plan[command->phase - 1].MaxGreen;

    if (command->effect_time < min_green) {
        command->effect_time = min_green;
    }

    if (command->effect_time > max_green) {
        command->effect_time = max_green;
    }

    pthread_mutex_lock(&mutex_command_buf);
    // 抓出要處理的cmd buff object
    //  comman->cycle and phase are used to "indicate the index of the
    //  target_command buffer object".
    tsc_command_object_t *target_command_obj =
        &command_buf[(cycle_index + command->cycle) % CYCLE_NUM]
                    [command->phase - 1];

    // command object first insert
    if (target_command_obj->app_id == 0) {
        target_command_obj->app_id = command->app_id;
        target_command_obj->app_priority = command->app_priority;
        target_command_obj->effect_time = command->effect_time;
        target_command_obj->compensation_time = command->compensation_time;
        // adjusted_time is the length of time that the "traffic signal
        // controller" is adjusted to.
        if (target_command_obj->adjusted_time == 0) {
            target_command_obj->adjusted_time =
                pretime;  // 因為此步階預設倒數時間為pretime
        }
        target_command_obj->target_phase = command->target_phase;
        target_command_obj->send_flag = false;
        strncpy(target_command_obj->host_OBU_name, command->host_OBU_name, 15);
        command_buf_print();
        pthread_mutex_unlock(&mutex_command_buf);
        return INSERT_ACCEPT;
    }

    // 如果target_command是補償指令的話，則取聯集。
    if (strncmp(target_command_obj->host_OBU_name, COMPENSATION_NAME,
                COMPENSATION_LEN) == 0) {
        if (strncmp(command->host_OBU_name, COMPENSATION_NAME, COMPENSATION_LEN) == 0) {
            printf("union compensation command\r\n");
            target_command_obj->compensation_time += command->compensation_time;
            target_command_obj->app_id = command->app_id;
            target_command_obj->app_priority = command->app_priority;
            target_command_obj->effect_time = target_command_obj->effect_time + (command->effect_time - pretime);
            target_command_obj->target_phase = command->target_phase;
            target_command_obj->send_flag = false;
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    OBU_NAME_MAX_LEN);
            command_buf_print();
            pthread_mutex_unlock(&mutex_command_buf);
            return INSERT_ACCEPT;
        }
    }

    // resume是為了強制回到pretime 怎麼作到？
    // evsp裡面會使用obu resumeid
    // replace resume command
    // 抓出來的目標cmd buff object其host obu id為resume id則優先取代？
    if (strncmp(target_command_obj->host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN) == 0) {
        if (strncmp(command->host_OBU_name, COMPENSATION_NAME, COMPENSATION_MAX_LEN) == 0) {
            if (command->compensation_cycle == 1) {
                tsc_command_object_t *target_compensation_command_obj =
                    &command_buf[(cycle_index + 1 + command->cycle) % CYCLE_NUM]
                                [command->phase - 1];
                target_compensation_command_obj->app_id = command->app_id;
                target_compensation_command_obj->app_priority = command->app_priority;
                target_compensation_command_obj->effect_time = command->effect_time;
                target_compensation_command_obj->target_phase = command->target_phase;
                target_compensation_command_obj->send_flag = false;
                strncpy(target_compensation_command_obj->host_OBU_name, command->host_OBU_name,
                        COMPENSATION_MAX_LEN);
                command_buf_print();
                pthread_mutex_unlock(&mutex_command_buf);
                return INSERT_ACCEPT;
            }
        }
        target_command_obj->app_id = command->app_id;
        target_command_obj->app_priority = command->app_priority;
        target_command_obj->effect_time = command->effect_time;
        target_command_obj->target_phase = command->target_phase;
        target_command_obj->send_flag = false;
        if (strncmp(command->host_OBU_name, COMPENSATION_NAME, COMPENSATION_MAX_LEN) == 0) {
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    COMPENSATION_MAX_LEN);
        } else {
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    OBU_NAME_MAX_LEN);
        }
        command_buf_print();
        pthread_mutex_unlock(&mutex_command_buf);
        return INSERT_ACCEPT;
    }

    // resume command
    if (strncmp(command->host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN) == 0) {
        if (target_command_obj->app_id == command->app_id) {
            target_command_obj->effect_time = command->effect_time;
            target_command_obj->target_phase = command->target_phase;
            target_command_obj->send_flag = false;
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    OBU_NAME_MAX_LEN);
            pthread_mutex_unlock(&mutex_command_buf);
            return INSERT_ACCEPT;
        } else {
            return IMPROPER_PRIORITY;
        }
    }

    // same OBU ID      appid的check看起來像是多餘的
    // 除非是一個obu有多個application This means that the application has made a
    // "new command for the serviced OBU". Replace the original command
    if (strncmp(target_command_obj->host_OBU_name, command->host_OBU_name,
                OBU_NAME_MAX_LEN) == 0 &&
        target_command_obj->app_id == command->app_id) {
        target_command_obj->target_phase = command->target_phase;
        target_command_obj->effect_time = command->effect_time;
        target_command_obj->send_flag = false;
        command_buf_print();
        pthread_mutex_unlock(&mutex_command_buf);
        return INSERT_ACCEPT;
    }
    // same target phase
    // Consider the effect_time of two commands and attempt to determine which
    // command can "serve two OBUs at the same time".
    if (target_command_obj->target_phase == command->target_phase) {
        // time difference between effect time & pretime increase
        if (abs(target_command_obj->effect_time - pretime) <=
            abs(command->effect_time -
                pretime)) {  // 變化差異要大於上一次的改變？不能縮短
            target_command_obj->app_id = command->app_id;
            target_command_obj->app_priority = command->app_priority;
            target_command_obj->effect_time = command->effect_time;
            target_command_obj->send_flag = false;
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    OBU_NAME_MAX_LEN);
            command_buf_print();
            pthread_mutex_unlock(&mutex_command_buf);
            return INSERT_ACCEPT;
        } else {
            command_buf_print();
            pthread_mutex_unlock(&mutex_command_buf);
            return IMPROPER_EFFECT_TIME;
        }
    } else {
        // 為甚麼會有不一樣target_phase問題？？？
        // TSP的target_phase不一定是目前的phase
        // 但EVSP的target_phase是目前的phase
        // 所以要判斷優先權大小來取代

        // different target phase
        // priority higher than original command 數值越小priority越高
        if (target_command_obj->app_priority >
            command->app_priority) {  // 優先權較小 tsp被evsp取代
            target_command_obj->app_id = command->app_id;
            target_command_obj->app_priority = command->app_priority;
            target_command_obj->effect_time = command->effect_time;
            target_command_obj->target_phase = command->target_phase;
            target_command_obj->send_flag = false;
            strncpy(target_command_obj->host_OBU_name, command->host_OBU_name,
                    OBU_NAME_MAX_LEN);
            command_buf_print();
            pthread_mutex_unlock(&mutex_command_buf);
            return INSERT_ACCEPT;
        } else {
            command_buf_print();
            pthread_mutex_unlock(&mutex_command_buf);
            return IMPROPER_PRIORITY;
        }
    }
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

    uint16_t pretime =
        signal_status.plan[command->phase - 1].PreTimeCompensated;
    uint16_t min_green = signal_status.plan[command->phase - 1].MinGreen;
    uint16_t max_green = signal_status.plan[command->phase - 1].MaxGreen;

    if (config.signal_adjust_lower_bound_active) {
        int16_t lower =
            pretime -
            pretime * config.signal_adjust_lower_bound_percentage * 0.01;
        min_green = (lower > min_green) ? lower : min_green;
    }

    if (config.signal_adjust_upper_bound_active) {
        int16_t upper =
            pretime +
            pretime * config.signal_adjust_upper_bound_percentage * 0.01;
        max_green = (upper < max_green) ? upper : max_green;
    }

    pthread_mutex_lock(&mutex_command_buf);
    // 抓出target phase的原始資料
    tsc_command_object_t *target_command_obj =
        &command_buf[(cycle_index + command->cycle) % CYCLE_NUM]
                    [command->phase - 1];

    // This means that this subphase has never been adjusted
    if (target_command_obj->adjusted_time == 0) {  // 第一次被調整？
        command->effect_time = pretime + command->adjustment;
    } else {
        command->effect_time =
            target_command_obj->adjusted_time + command->adjustment;
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

    int ret = 0;
    ret = command_buf_insert_effect_time(command);
    return ret;
}

void command_buf_print()
{  // 就要不要log command buffer的開關
    if (config.log_command_buffer == 0) {
        return;
    }
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "command buffer: current cycle index (%d)", cycle_index);
    for (int i = 0; i < CYCLE_NUM; i++) {
        for (int j = 0; j < signal_status.SubPhaseCount; j++) {
            snprintf(
                log_content + strlen(log_content),
                LOG_CONTENT_LEN - strlen(log_content),
                "\ncmd[%d][%d]: AT:%3d, PT:%3d, HoID:%-15s, TP:%1d, AppID:%2d, "
                "AppPri:%2d, ET:%3d, SF:%1d",
                i, j + 1, command_buf[i][j].adjusted_time,
                signal_status.plan[j].PreGreen, command_buf[i][j].host_OBU_name,
                command_buf[i][j].target_phase, command_buf[i][j].app_id,
                command_buf[i][j].app_priority, command_buf[i][j].effect_time,
                command_buf[i][j].send_flag);
        }
    }
    log_file_write(log_content);
    return;
}

bool check_command_buf_empty()
{
    for (int i = 0; i < CYCLE_NUM; i++) {
        for (int j = 0; j < SUBPHASEID_NUM; j++) {
            if (command_buf[i][j].app_id != 0)
                return false;
        }
    }
    return true;
}