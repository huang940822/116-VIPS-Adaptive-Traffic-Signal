#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/timerfd.h>

#include "EVSP.h"
#include "EVSP_OBU_list.h"
#include "EVSP_subphase_threshold.h"
#include "EVSP_touching_area.h"
#include "config.h"
#include "gps_information.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

#define MIN_EXPECT_SPEED 15  // 15 m/s 54 km/hr

#define THESHOLD_BUFFER 5

LOG_USE_MODULE(EVSP);

// EVSP 一次只會服務一台 OBU
// activate_OBU 最多只會有一個 thread 服務一台 OBU
EVSP_activate_OBU_t activate_OBU = {
    .host_OBU_name = {0},
    .activate_mutex = PTHREAD_MUTEX_INITIALIZER,
    .activate_thread = 0,
    .control_subphaseID = 0,
    .list_len = 0,
    .target_phase = 0,
};

EVSP_activate_OBU_t *activate_OBU_head = NULL;
void init_activate_OBU_head() {
    activate_OBU_head = (EVSP_activate_OBU_t*)malloc(sizeof(EVSP_activate_OBU_t));
    if (activate_OBU_head == NULL) {
        perror("Failed to allocate memory for activate_OBU_head");
        return;
    }

    memset(activate_OBU_head->host_OBU_name, '\0', sizeof(activate_OBU_head->host_OBU_name));
    activate_OBU_head->activate_thread = 0;
    activate_OBU_head->control_subphaseID = 0;
    activate_OBU_head->target_phase = 0;
    activate_OBU_head->prev = NULL;
    activate_OBU_head->next = NULL;

    if (pthread_mutex_init(&activate_OBU_head->activate_mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(activate_OBU_head);
        activate_OBU_head = NULL;
        return;
    }
}

/**
 * 使用其他各分項最短綠與黃燈紅燈總和(快速輪轉)加上約20秒緩衝作為延長綠燈秒數Gmx
 * 若預計抵達時間超過Gmx表示使用快速輪轉即可，不需要用延長解
 */

int EVSP_extend_formula(int target_phase, traffic_signal_status_t *signal_status, int Tbf)
{
    int EVSP_adjust_time = 0;
    // Ｇmx＝ΣＰnb＋Ｔbf
    // Ｇmx：延長最長綠燈秒數
    // ΣＰnb：非公車各分相最短綠及清道時間總和
    // Ｔbf：緩衝誤差值，約為１０-２０秒，視觸動之距離而調整
    // 路口號誌為三時相，週期Ｃ為１２０秒，第一時相為公車方向６４秒綠燈、４秒黃燈、２秒全紅；
    // 第二時相１０秒綠燈、３秒黃燈、２秒全紅；第三時相２９秒綠燈、３秒黃燈、３秒全紅；而第二時相最短綠為５秒、第三時相最短綠為１５秒
    // Ｇmx＝【（５＋３＋２）＋（１５＋３＋３）】
    // 分相若是行人專屬時相 yellow = 0

    for (int i = 1; i < 8; i++) {
        if (i != target_phase)
            EVSP_adjust_time += signal_status->plan[i - 1].MinGreen +
                                signal_status->plan[i - 1].Yellow +
                                signal_status->plan[i - 1].AllRed;
    }
    return EVSP_adjust_time += Tbf;  // Gmx += 20 ，緩衝誤差值調最大
}

float get_expect_arrival_time(EVSP_host_OBU_obj_t *host_OBU)
{
    return (get_distance(config.RSU_lat, config.RSU_lon, host_OBU->lat, host_OBU->lon) /
            (host_OBU->speed > MIN_EXPECT_SPEED ? host_OBU->speed : MIN_EXPECT_SPEED)) -
           THESHOLD_BUFFER;
}

float get_subphase_threshold(int target_phase, float expect_arrival_time, traffic_signal_status_t *signal_status)
{
    return expect_arrival_time - EVSP_extend_formula(target_phase, signal_status, THESHOLD_BUFFER);
}

/*
幹道的分相為主要分相
如果 target phase 在主要分相使用短時間快速輪轉
如果不是使用長時間平滑控制
*/
int EVSP_optimization(EVSP_host_OBU_obj_t *host_OBU, traffic_signal_status_t *signal_status, char *log_content)
{
    int adjust_time = -1;
    int current_subPhase = signal_status->SubPhaseID;
    int current_step = signal_status->StepID;

    // 只有在步階一才能調整
    if (current_step != 1)
        return adjust_time;
    int target_phase = host_OBU->target_phase;
    float expect_arrival_time = get_expect_arrival_time(host_OBU);
    float subphase_threshold = get_subphase_threshold(target_phase, expect_arrival_time, signal_status);


    EVSP_plan_table_t *plan = EVSP_plan_table_search(signal_status->PlanID);
    uint16_t pretime = signal_status->plan[current_subPhase - 1].PreGreen;
    int shorten_estimate = 0, extend_estimate = 0;
    int target_green = signal_status->plan[target_phase - 1].Green;
    int subPhase_time = signal_status->StepSec + signal_status->plan[current_subPhase - 1].PedGreenFlash +
                        signal_status->plan[current_subPhase - 1].PedRed + signal_status->plan[current_subPhase - 1].Yellow +
                        signal_status->plan[current_subPhase - 1].AllRed;
    bool is_main_subphase = true;  // 是不是主要分相


    LOG_MSG_APPEND(log_content,
                 "\ncurrent_subPhase %d target_phase %d\n"
                 "expect_arrival_time %.01f subphase_threshold %.01f\n"
                 "vehicle prediction speed %.2f, MIN_EXPECT_SPEED %d\n",
                 current_subPhase, target_phase, expect_arrival_time, subphase_threshold, host_OBU->speed, MIN_EXPECT_SPEED);

    // 判斷是不是主要分相 (支道或幹道)
    for (int i = 0; i < signal_status->SubPhaseCount; i++) {
        int subPhaseID = plan->plan_subPhase[i].SubPhaseID;
        LOG_MSG_TRACE("target_green %d signal_status->plan[subPhaseID - 1].Green %d", target_green, signal_status->plan[subPhaseID - 1].Green);
        if (subPhaseID != target_phase && target_green <= signal_status->plan[subPhaseID - 1].Green) {
            is_main_subphase = false;
            break;
        }
    }

    if (is_main_subphase == false) {
        // 目標分相為次要分相使用長時間平滑控制 (目標分相在支道)
        // 如果有兩個以上的綠燈時間長度一樣代表兩向的權重一樣，這時候使用短時間快速輪轉
        LOG_MSG_APPEND(log_content, "Long time smooth control\n");
        int BeforeMtargetSubPhase = 0;
        int accumulation_target = 0;
        // 尋找主目標分相
        for (int i = (current_subPhase - 1);
             ((i != (target_phase - 1)) || (subphase_threshold > (accumulation_target + subPhase_time))) && BeforeMtargetSubPhase < 10;) {
            LOG_MSG_TRACE("subphase_threshold %f accumulation_target + subPhase_time %d", subphase_threshold, accumulation_target + subPhase_time);
            accumulation_target += subPhase_time;
            i = (i + 1) % signal_status->SubPhaseCount;
            subPhase_time = signal_status->plan[i].Green + signal_status->plan[i].Yellow + signal_status->plan[i].AllRed;
            BeforeMtargetSubPhase++;
        }
        LOG_MSG_APPEND(log_content, "main target subphase start %d, end %d\n", accumulation_target, accumulation_target + subPhase_time);
        if (BeforeMtargetSubPhase >= 10) {
            LOG_MSG_APPEND(log_content, "BeforeMtargetSubPhase over limit, %d\n", BeforeMtargetSubPhase);
            return -1;
        }
        if (BeforeMtargetSubPhase == 0) {  // 在主目標分相
            adjust_time = pretime + EVSP_extend_formula(target_phase, signal_status, 20);
            LOG_MSG_APPEND(log_content, "extend main target subphase, extend %d\n", adjust_time);
        } else {
            LOG_MSG_APPEND(log_content, "BeforeMtargetSubPhase %d\n", BeforeMtargetSubPhase);
            if (expect_arrival_time < accumulation_target) {
                    // 預期抵達時間比主目標分相開始早
                    // 所以進行縮短
                    int remain = 0;
                    shorten_estimate = ceil(((float) accumulation_target - expect_arrival_time) / BeforeMtargetSubPhase);
                    // 如果其他分相不夠扣會優先從現在分相扣
                    for (int i = (current_subPhase - 1), j = 1; j < BeforeMtargetSubPhase; j++) {
                        i = (i + 1) % signal_status->SubPhaseCount;
                        if (signal_status->plan[i].PreGreen - signal_status->plan[i].MinGreen < shorten_estimate)
                            remain += (shorten_estimate - (signal_status->plan[i].PreGreen - signal_status->plan[i].MinGreen));
                    }
                    adjust_time = pretime - shorten_estimate - remain;
                    adjust_time = adjust_time < 0 ? 0 : adjust_time;  // 避免小於 0
                    LOG_MSG_APPEND(log_content, "arrive time is too early, shorten %d(+%d)\n", shorten_estimate, remain);
            } else if (accumulation_target + subPhase_time < expect_arrival_time) {
                // 比主目標分相結束晚，所以進行延長
                extend_estimate = ceil((expect_arrival_time - (accumulation_target + subPhase_time)) / BeforeMtargetSubPhase);
                adjust_time = pretime + extend_estimate;
                LOG_MSG_APPEND(log_content, "arrive time is too late, extend %d\n", extend_estimate);
            } else {
                // 預期抵達時間在主目標分相時段內就甚麼都不做
                LOG_MSG_APPEND(log_content, "arrive time is in main target subphase\n");
                adjust_time = pretime;
            }
        }

    } else {
        // 目標分相為主要分相使用短時間快速輪轉 (目標分相在幹道)
        LOG_MSG_APPEND(log_content, "short time quick control\ncursubphase end %d subphase_threshold %.2f\n",
                     subPhase_time, subphase_threshold);
        if (subPhase_time >= subphase_threshold) {
            // 如果現在的時相
            if (target_phase == current_subPhase) {
                adjust_time = pretime + EVSP_extend_formula(target_phase, signal_status, 20);
                LOG_MSG_APPEND(log_content, "extend main target subphase\n");
            } else {
                adjust_time = 0;
                LOG_MSG_APPEND(log_content, "change to next subphase\n");
            }
        } else {
            LOG_MSG_APPEND(log_content, "current subphase out of the subphase_threshold\n");
            adjust_time = pretime;
        }
    }
    LOG_MSG_APPEND(log_content, "adjust_time %d\n", adjust_time);
    return adjust_time;
}

void *EVSP_OBU_activation_timer()
{
    int fd = set_timer_fd(1, "EVSP_OBU_activation_timer");
    int optim = 1;      //  1 表示要做optimization (有 activate OBU 加入或離開 or 時相變換都要做)
    int old_list_len = 0;   //  記錄前一輪 activate list長度，當activate.list_len != old_list_len 表示有 activate OBU 加入或離開
    traffic_signal_status_t signal_status;
    tsc_command_t command = {0};
    EVSP_touching_area_t *area_ptr = NULL;
    EVSP_host_OBU_obj_t *host_OBU;

    char log_content[LOG_CONTENT_LEN + 1] = {0};

    if (fd == -1) {
        return NULL;
    }

    //把activate資訊複製進去command中準備執行
    command.app_id = EVSP.id;
    command.app_priority = EVSP.priority;


    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            LOG_MSG_FATAL("EVSP_OBU_activation_timer timer read error %d", s);

        memset(log_content, 0, sizeof(log_content));
        get_traffic_signal_status(&signal_status);
        // 獲取交通訊號狀態並嘗試查找對應的計劃表。如果計劃表找不到，則記錄錯誤並跳出。
        EVSP_plan_table_t *plan = EVSP_plan_table_search(signal_status.PlanID);
        int ret = -1;
        if (plan == NULL) {
            LOG_MSG_APPEND(log_content, "EVSP_OBU_activation_timer touching area plan not found");
            LOG_MSG_INFO(log_content);
            break;
        }

        //terminate area後OBU會被刪除，thread這邊就會進行釋放
        pthread_mutex_lock(&activate_OBU.activate_mutex);
        pthread_mutex_lock(&activate_OBU_head->activate_mutex);
        EVSP_activate_OBU_t *tmp = activate_OBU_head;
        // pthread_mutex_lock(&tmp->activate_mutex);
        while(activate_OBU_head != NULL){
            host_OBU = EVSP_host_OBU_obj_search(activate_OBU_head->host_OBU_name, NULL);
            EVSP_host_OBU_obj_print();
            //如果查不到代表已經terminate了(執行過EVSP_host_OBU_obj_delete)
            if (host_OBU == NULL){
                //代表此activate head是最後一台且已經terminated，結束thread
                if (activate_OBU_head->next == NULL){
                    LOG_MSG_APPEND(log_content, "last one terminated\nhost_OBU_name = %s", tmp->host_OBU_name);
                    activate_OBU_head = activate_OBU_head->next;
                    free(tmp);
                    activate_OBU.list_len -- ;
                    pthread_mutex_unlock(&activate_OBU.activate_mutex);
                    goto thread_end;
                }
                else{   //有下一個OBU就更換acitivate head
                    activate_OBU_head = activate_OBU_head->next;
                    free(tmp);
                    activate_OBU.list_len -- ;
                    tmp = activate_OBU_head;
                }
                optim = 1; //有更換就要做optimization
            }
            else{
                break;
            }
        }

        //有增刪activate_OBU也要再重optimization
        if (activate_OBU.list_len != old_list_len || signal_status.SubPhaseID != activate_OBU_head->control_subphaseID)
            optim = 1;

        //如果要optimization，要更新command.host_OBU_name
        if (optim == 1){
            strncpy(command.host_OBU_name, activate_OBU_head->host_OBU_name, sizeof(activate_OBU_head->host_OBU_name));
        }

        // 如果當前subphaseID與thread的subphaseID一致且未有變動，且 activate list 也沒有變化，則不執行任何操作，繼續下一次
        if (signal_status.SubPhaseID == activate_OBU_head->control_subphaseID && optim == 0) {
            LOG_MSG_APPEND(log_content, "do nothing");
            LOG_MSG_INFO(log_content);
            pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
            pthread_mutex_unlock(&activate_OBU.activate_mutex);
            continue;
        }

        if (optim == 1){
            tmp = activate_OBU_head;
            int max_adjust_time = -1, min_adjust_time = 1000;
            /*
             * 對所有 activate OBU 做 optimization
             * 並取最大和最小 (最大會是請求延長的OBU，最小是請求縮短的OBU)
             * 其餘情況都是以延長為優先
             *  */
            while (tmp != NULL){
                host_OBU = EVSP_host_OBU_obj_search(tmp->host_OBU_name, NULL);
                ret = EVSP_optimization(host_OBU, &signal_status, log_content);
                max_adjust_time = (ret >= max_adjust_time) ? ret : max_adjust_time;
                min_adjust_time = (ret < min_adjust_time && ret != -1) ? ret : min_adjust_time;
                tmp = tmp->next;
            }

            optim = 0;
            old_list_len = activate_OBU.list_len;

            if (max_adjust_time != -1 && min_adjust_time != -1) {
                command.target_phase = host_OBU->target_phase;
                command.phase = signal_status.SubPhaseID;
                /**
                 * 依照情境調整 effect time
                 * 若 max_adjust_time 不比目前綠燈秒數 preGreen 大(表示沒有車要請求延長)，才處理縮短請求，以min_adjust_time調整
                 */
                command.effect_time = (signal_status.plan[signal_status.SubPhaseID - 1].PreGreen >= max_adjust_time) ? min_adjust_time : max_adjust_time;
                ret = command_buf_insert_effect_time(&command);

                LOG_MSG_APPEND(log_content, "\ncycle: %d, phase: %d, effect time: %d (%d)",
                            command.cycle, command.phase, command.effect_time, ret);
                if (ret != -1) {
                    tmp = activate_OBU_head;
                    while (tmp != NULL){
                        // 將 conttrol_subphaseID 調整成和 signal_status 相同
                        tmp->control_subphaseID = signal_status.SubPhaseID;
                        tmp = tmp->next;
                    }
                }
            }
        }
        pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
        pthread_mutex_unlock(&activate_OBU.activate_mutex);
        if (ret != -1) {
            EVSP_host_OBU_obj_print();
        }
        LOG_MSG_INFO(log_content);
    }
    thread_end:
        pthread_mutex_lock(&activate_OBU.activate_mutex);
        activate_OBU.activate_thread = 0;
        activate_OBU.control_subphaseID = 0;
        activate_OBU.list_len = 0;
        memset(activate_OBU.host_OBU_name, 0, sizeof(activate_OBU.host_OBU_name));
        pthread_mutex_unlock(&activate_OBU.activate_mutex);
        LOG_MSG_APPEND(log_content, "EVSP_OBU_activation_timer close\n");
        LOG_MSG_INFO(log_content);
        close(fd);
        pthread_detach(pthread_self());
}
/**
 * 負責
 * 1.   檢查 host_OBU 與 activate_OBU_head 的同相關係及註冊情形判斷
 * 2.   符合條件之 host_OBU 加入 activate list ，並為首個 activate OBU 創造執行秒數 optimization 的 thread
 * 3.   return value
 *      -1 : host_OBU 與 activate_OBU_head 不同相或是已經在 activate list 中了
 *       1 : 首個 activate OBU 加入，需 create thread 並觸動 CMS
 *       2 : 符合條件之同相 OBU ，加入activate list 但不做 CMS 的觸動
 */
int EVSP_OBU_activation_timer_start(EVSP_host_OBU_obj_t *host_OBU)
{
    char log_content[LOG_CONTENT_LEN + 1] = {0};
    if (host_OBU == NULL)
        return -1;
    pthread_mutex_lock(&activate_OBU.activate_mutex);
    if (activate_OBU.activate_thread == 0){
        init_activate_OBU_head();   // 初次執行 thread 要初始化 activate_OBU_head
        LOG_MSG_APPEND(log_content, "init activate head success\n");
    }
    //如果已經有執行中的 OBU，需要判斷目前的 host_OBU 和 activate_OBU_head 是否同相和是否已經加入 activate 的行列
    else{
        pthread_mutex_lock(&activate_OBU_head->activate_mutex);
        //  不同相就退出跳過
        if (activate_OBU_head->target_phase != host_OBU->target_phase) {
            LOG_MSG_INFO(log_content);
            pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
            pthread_mutex_unlock(&activate_OBU.activate_mutex);
            return -1;
        }

        //  已經在 activate OBU list 的也跳過
        if (search_activate_OBU(host_OBU) != NULL){
            LOG_MSG_INFO(log_content);
            pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
            pthread_mutex_unlock(&activate_OBU.activate_mutex);
            return -1;
        }
        pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
    }

    add_activate_OBU(host_OBU);
    activate_OBU.list_len++;
    LOG_MSG_APPEND(log_content, "add activate OBU success\nhost_OBU_name = %s\n", host_OBU->OBU_name);
    //如果是第一個 activate 的就 create timer thread
    if (activate_OBU.activate_thread == 0) {
        int ret = pthread_create(&activate_OBU.activate_thread, NULL, EVSP_OBU_activation_timer, NULL);
        if (ret != 0) {
            LOG_MSG_APPEND(log_content, "Failed to create thread\n");
            LOG_MSG_INFO(log_content);
            pthread_mutex_unlock(&activate_OBU.activate_mutex);
            return -1;
        }
        LOG_MSG_INFO(log_content);
        pthread_mutex_unlock(&activate_OBU.activate_mutex);
        return 1; //return 1 才開啟CMS
    }
    LOG_MSG_INFO(log_content);
    pthread_mutex_unlock(&activate_OBU.activate_mutex);
    return 2;
}

/**
 * 當 terminate 的是 activate_OBU_head 及是最後一台車之結束判斷
 * terminate_flag
 *  0: 當terminate 的不是 activate_OBU_head 就不動作
 *  1: 當terminate 的是 activate_OBU_head 且 activate list 尚有其他 OBU，需要變更 CMS
 *  2: 當terminate 的是 activate_OBU_head 且已是最後一個 OBU，不需再重啟 CMS
 */
int EVSP_OBU_activation_time_end(char *OBU_name)
{
    int terminate_flag = 0;
    pthread_mutex_lock(&activate_OBU.activate_mutex);
    pthread_mutex_lock(&activate_OBU_head->activate_mutex);
    // 如果 terminate 的是 activate_head，就設定 flag 為 1，表示需要再重新啟動CMS
    if (strncmp(OBU_name, activate_OBU_head->host_OBU_name, sizeof(activate_OBU_head->host_OBU_name)) == 0) {
        terminate_flag = 1;
        if(activate_OBU_head->next == NULL) //  若 activate_OBU_head 是最後一台就不用再重啟 CMS 了，設定為2
            terminate_flag = 2;
    }
    if (activate_OBU.list_len == 0) //  若 activate list 已全部 terminate 就不用再重啟 CMS 了，設定為2
        terminate_flag = 2;
    pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
    pthread_mutex_unlock(&activate_OBU.activate_mutex);
    return terminate_flag;
}

//搜尋 host_OBU 是否已經在 activate_OBU 的 list 中
EVSP_activate_OBU_t* search_activate_OBU(EVSP_host_OBU_obj_t *host_OBU) {
    EVSP_activate_OBU_t *current = activate_OBU_head;
    while (current != NULL) {
        if (strncmp(current->host_OBU_name, host_OBU->OBU_name, sizeof(current->host_OBU_name)) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void add_activate_OBU(EVSP_host_OBU_obj_t *host_OBU) {

    EVSP_activate_OBU_t *new_activate_OBU = (EVSP_activate_OBU_t*)malloc(sizeof(EVSP_activate_OBU_t));
    if (new_activate_OBU == NULL)
        return;

    if (pthread_mutex_init(&new_activate_OBU->activate_mutex, NULL) != 0) {
        free(new_activate_OBU);
        return;
    }
    pthread_mutex_lock(&activate_OBU_head->activate_mutex);
    pthread_mutex_lock(&new_activate_OBU->activate_mutex);
    strncpy(new_activate_OBU->host_OBU_name, host_OBU->OBU_name, sizeof(new_activate_OBU->host_OBU_name));
    new_activate_OBU->activate_thread = 0;
    new_activate_OBU->list_len = activate_OBU.list_len + 1;
    new_activate_OBU->control_subphaseID = 0;
    new_activate_OBU->target_phase = host_OBU->target_phase;
    new_activate_OBU->next = NULL;
    new_activate_OBU->prev = NULL;
    //  如果是第一個 activate 的 OBU ，就把 activate_OBU_head 指向該 OBU

    if (strlen(activate_OBU_head->host_OBU_name) == 0) {
        activate_OBU_head = new_activate_OBU;
    } else {
        EVSP_activate_OBU_t *temp = activate_OBU_head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_activate_OBU;
        new_activate_OBU->prev = temp;
    }
    pthread_mutex_unlock(&new_activate_OBU->activate_mutex);
    pthread_mutex_unlock(&activate_OBU_head->activate_mutex);
}