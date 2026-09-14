#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <libwebsockets.h>
#include <signal.h>
#include <pthread.h> 
#include <socket_receiver.h>

#include "PEDESTRIAN_plus.h" 
#include "PEDESTRIAN_timer_event.h"
#include "ObstacleList.h"
#include "Pedestrian.h"
#include "buffer.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "traffic_signal_packet_tx.h"
#include "typedefine.h"

// 矩陣
#define cols 11
#define rows 8 
#define waiting_rows 4
// 燈號原始參數
#define G 30.0 //綠
#define GF 27.0 //綠閃
#define Y 2.0 //黃
#define R 2.0 //紅
#define PT 65.0 // 總秒數

#define MESSAGE_SIZE 128

// 結構定義
typedef struct {
    double x;
    double y;
} Point;

// 互斥鎖
static pthread_mutex_t ws_mutex_plus = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t matrix_mutex_plus = PTHREAD_MUTEX_INITIALIZER;

// VIPS_plus 演算法變數
int CS_plus = 2; //Current Stage 
int direction_flag_plus = 0; 
double Y_plus, LG_plus, LGF_plus, UR_plus, LR_plus, ER_plus;
double dynamic_G_plus = 0.0, dynamic_GF_plus = 0.0; 
int stage_timers_plus[7] = {0}; 
int PIC_plus = 0; //行人總數
int waiting_count = 0;

// 系統狀態
bool enter_ped_phase_plus = false;
bool time_flag_plus = false;
int second_plus = 0;
timer_t pedestrian_timer_id_plus;
time_t last_update_time_plus[7]; 

// 矩陣與幾何資訊
int matrix_plus[rows][cols] = { 0 };
int waiting_area_plus[waiting_rows] = { 0 }; //獨立的等待區陣列

//已調
static double road_length_plus[7] = { 0,0,0,198.204,259.788,176.011,297.29};
//實際座標
static double P_start_x_plus[7] = { 0,0,0,620,605,623,605 };
static double P_start_y_plus[7] = { 0,0,0,232.5,321.5,355,435 };
static double P_end_x_plus[7] = { 0,0,0,330,260,250,235 };
static double P_end_y_plus[7] = { 0,0,0,135,190,127.5,66 };

//sumo 座標
// static double P_start_x_plus[7] = { 0,0,0,899.14,912.15,914.63,897.175 };
// static double P_start_y_plus[7] = { 0,0,0,1040.73,1023.69,1025.24,1039.08 };
// static double P_end_x_plus[7] = { 0,0,0,912.15,899.14,914.63,897.175 };
// static double P_end_y_plus[7] = { 0,0,0,1040.73,1023.69,1039.08,1025.24 };


#define MAX_CLIENTS 10

struct lws_context* context_plus = NULL; 
static struct lws* clients_plus[MAX_CLIENTS];
static int client_count_plus = 0;
static char message_buffer_plus[MESSAGE_SIZE] = "Red"; 
char log_content_plus[LOG_CONTENT_LEN + 1];

//函數宣告
double calculate_projection_on_road_plus(Point P, Point P_start, Point P_end);
bool is_anyone_before_ratio_plus(int ratio_cols);
void start_websocket_server_plus();
extern void sumo_switch_phase();//sumo


//初始化變數
void init_variable_plus(void) {
    Y_plus = Y; //黃燈時限不變
    LG_plus = (G + GF) * 0.2; //最小綠燈時限        
    LGF_plus = (G + GF) * 0.4; //最小綠閃時限       
    UR_plus = (G + GF + R) - LG_plus - LGF_plus; //最大紅燈時限
    LR_plus = 2.0; //最大紅燈時限                   
    ER_plus = PT * 0.1; //延長全紅上限            
    
    CS_plus = 1; //最初始為stage1
    direction_flag_plus = 0;
    
    for(int i=0; i<7; i++) stage_timers_plus[i] = 0;
    
    dynamic_G_plus = G; 
    dynamic_GF_plus = GF;
    fprintf(stderr, "[VIPS+] Initial: LG=%.1f, LGF=%.1f, ER=%.1f, UR_plus=%.1f\n", LG_plus, LGF_plus, ER_plus, UR_plus);
}

//計算矩陣總人數
void calculateMaxPed_plus(void) {
    PIC_plus = 0;
    for (int i = 0; i < rows; i++) {
        for (int j = 1; j < cols; j++) {
            PIC_plus += matrix_plus[i][j]; 
            // fprintf(stderr, "%d ", matrix_plus[i][j]);
        }
        // fprintf(stderr, "\n");
    }
}

//計算等待區人數
void calculateWaiting(void) {
    waiting_count = 0;
    // for (int i = 0; i < waiting_rows; i++) {
    //     waiting_count += waiting_area_plus[i]; // 直接使用獨立的等待區陣列
    // }
    // fprintf(stderr, "[VIPS+] %d %d %d %d\n", matrix_plus[0][0], matrix_plus[2][0], matrix_plus[4][0], matrix_plus[6][0]);
    waiting_count += (matrix_plus[1][0] + matrix_plus[3][0] + matrix_plus[5][0] + matrix_plus[7][0]); 
}

//檢查特定比例的人數
bool is_anyone_before_ratio_plus(int ratio_cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 1; j <= ratio_cols; j++) {
            if (matrix_plus[i][j] > 0) {
                fprintf(stderr, "[VIPS+] Detected %d pedestrians in  row %d,  ratio %d.\n", matrix_plus[i][j], i, j);
                return true; 
            }
        }
    }
    return false; 
}

// VIPS_plus 狀態
int runPedestrianSignalControl_plus(void) {
    // fprintf(stderr, "[TRACE] Current context_plus ptr: %p\n", (void*)context_plus);
    calculateMaxPed_plus(); // 更新 PIC_plus
    
    fprintf(stderr, "[VIPS+] CS=%d, Dir=%d, PIC=%d\n", CS_plus, direction_flag_plus, PIC_plus);
    switch (CS_plus) {
        case 1: // Stage 1 
            stage_timers_plus[1]++;
            calculateWaiting(); // 更新 waiting_count
            fprintf(stderr, "[VIPS+] Stage 1 Timer: %d seconds\n", stage_timers_plus[1]);
            if (stage_timers_plus[1] < 5) { // 行綠早開最多5秒
                if (stage_timers_plus[1] == 1) {
                    fprintf(stderr, "[VIPS+] waiting_count: %d\n", waiting_count);
                    if (waiting_count <= 5) {  //低 5人以下
                        dynamic_GF_plus = (G + GF) * 0.5;
                        dynamic_G_plus = (G + GF) - dynamic_GF_plus;
                        fprintf(stderr, "[VIPS+] Flow: LOW (PIC=%d) -> G=%.1f, GF=%.1f\n", PIC_plus, dynamic_G_plus, dynamic_GF_plus);
                        if (waiting_count == 0) {
                            CS_plus = 2;
                            fprintf(stderr, "[VIPS+] No Pedestrians in Stage 1. Skipping Early Start.\n");
                        }
                    } 
                    else if (waiting_count <= 30) { //中 6~30人
                        dynamic_GF_plus = (G + GF) * 0.65;
                        dynamic_G_plus = (G + GF) - dynamic_GF_plus;
                        fprintf(stderr, "[VIPS+] Flow: MID (PIC=%d) -> G=%.1f, GF=%.1f\n", PIC_plus, dynamic_G_plus, dynamic_GF_plus);
                    } 
                    else {  //高 >=31人
                        dynamic_GF_plus = (G + GF) * 0.75;
                        dynamic_G_plus = (G + GF) - dynamic_GF_plus;
                        fprintf(stderr, "[VIPS+] Flow: HIGH (PIC=%d) -> G=%.1f, GF=%.1f\n", PIC_plus, dynamic_G_plus, dynamic_GF_plus);
                    }
                }
            } else {
                CS_plus = 2; 
            }
            break;

        case 2: // Stage 2 行人綠燈
            stage_timers_plus[2]++;

            fprintf(stderr, "[VIPS+] Stage 2 Timer: %d seconds\n", stage_timers_plus[2]);
            if (stage_timers_plus[2] >= LG_plus) {
                // bool ped_still_at_start = is_anyone_before_ratio_plus(2);
                bool ped_still_at_start = is_anyone_before_ratio_plus(3);
                if (stage_timers_plus[2] > LG_plus) {
                    UR_plus--;
                    fprintf(stderr, "\033[1;32m[VIPS+] over LG_plus\033[0m\n");
                }
                if (!ped_still_at_start) {
                    CS_plus = 3;
                } else {
                    if (stage_timers_plus[2] >= dynamic_G_plus) {
                        fprintf(stderr, "\033[1;32m[VIPS+] Pedestrians still at start after dynamic G. Transitioning to Stage 3\033[0m\n");
                         CS_plus = 3;
                    }
                }
            }
            break;

        case 3: // Stage 3 行人綠閃
            stage_timers_plus[3]++;

            fprintf(stderr, "[VIPS+] Stage 3 Timer: %d seconds\n", stage_timers_plus[3]);
            if (stage_timers_plus[3] >= LGF_plus) {
                // bool ped_still_on_road = is_anyone_before_ratio_plus(9);
                bool ped_still_on_road = is_anyone_before_ratio_plus(10);
                if (stage_timers_plus[3] > LGF_plus) {
                    UR_plus--;
                    fprintf(stderr, "\033[1;32m[VIPS+] over LGF_plus\033[0m\n");
                }
                // fprintf(stderr, "UR_plus=%.1f\n", UR_plus);
                if (!ped_still_on_road) {
                    CS_plus = 4;
                } else {
                    if (stage_timers_plus[3] >= dynamic_GF_plus) {
                        fprintf(stderr, "\033[1;32m[VIPS+] Pedestrians still on road after dynamic GF. Transitioning to Stage 4\033[0m\n");
                        CS_plus = 4;
                    }
                }
            }
            break;

        case 4: // Stage 4 行人紅/車綠
            stage_timers_plus[4]++;
            
            fprintf(stderr, "[VIPS+] Stage 4 Timer: %d seconds, UR=%.1f\n", stage_timers_plus[4], UR_plus);
            if (stage_timers_plus[4] >= UR_plus) {
                CS_plus = 5;
            }
            break;
        
        case 5: // Stage 5 行人紅/車黃
            stage_timers_plus[5]++;

            fprintf(stderr, "[VIPS+] Stage 5 Timer: %d seconds\n", stage_timers_plus[5]);
            if (stage_timers_plus[5] >= Y_plus) {
                CS_plus = 6;
            }
            break;
        
        case 6: // Stage 6 全紅&延長全紅
            stage_timers_plus[6]++;

            fprintf(stderr, "[VIPS+] Stage 6 Timer: %d seconds\n", stage_timers_plus[6]);
            if (stage_timers_plus[6] >= LR_plus) {
                fprintf(stderr, "\033[1;31m[VIPS+] Reached LR_plus. Checking if we can switch direction.\033[0m\n");
                if (PIC_plus > 0) {
                    if (stage_timers_plus[6] >= (LR_plus + ER_plus)) {
                        fprintf(stderr, "\033[1;31m[VIPS+] Reached ER_plus with PIC=%d. Forcing direction switch.\033[0m\n", PIC_plus);
                        goto SWITCH_DIRECTION;
                    }
                } else {
                    goto SWITCH_DIRECTION; //轉換東西向與南北向
                }
            }
            break;
            
        SWITCH_DIRECTION:
            direction_flag_plus = !direction_flag_plus;
            fprintf(stderr, "[VIPS+] Cycle End. Switching Direction to %d. Resetting to Stage 1.\n", direction_flag_plus);
            
            for(int i=0; i<7; i++) stage_timers_plus[i] = 0;
            UR_plus = (G + GF + R) - LG_plus - LGF_plus; //重置 UR_plus
            CS_plus = 1; 
            break;
    }

    //更新 WebSocket 訊息
    pthread_mutex_lock(&ws_mutex_plus); //鎖

    const char* active_ped_state = "Red";
    const char* active_veh_state = "Red";

    switch (CS_plus) {
        case 1: active_ped_state = "Green";      active_veh_state = "Red";    break;
        case 2: active_ped_state = "Green";      active_veh_state = "Green";  break;
        case 3: active_ped_state = "GreenFlash"; active_veh_state = "Green";  break;
        case 4: active_ped_state = "Red";        active_veh_state = "Green";  break;
        case 5: active_ped_state = "Red";        active_veh_state = "Yellow"; break;
        case 6: active_ped_state = "Red(準備中 行人放行)";        active_veh_state = "Red";    break;
        default: break;
    }

    //判斷當前方向(SubPhase 1,2 是南北向，3,4 是東西向)
    const char* dir_str = (direction_flag_plus == 0) ? "North/South" : "East/West";

    //字串，格式是(行人燈 | 車行燈 | 方向 | 分時相-步階)
    snprintf(message_buffer_plus, MESSAGE_SIZE, "%s | %s | %s | %d-%d", 
             active_ped_state, active_veh_state, dir_str, get_current_phase(), get_current_step());

    if (context_plus != NULL) { 
        for (int i = 0; i < client_count_plus; i++) {
            if(clients_plus[i]) lws_callback_on_writable(clients_plus[i]);
        }
        lws_cancel_service(context_plus);
    }
    
    pthread_mutex_unlock(&ws_mutex_plus); 

    return 0;
}

// 通訊與系統架構

//計算投影比例
double calculate_projection_on_road_plus(Point P, Point P_start, Point P_end) {
    double length_squared = (P_end.x - P_start.x) * (P_end.x - P_start.x) + (P_end.y - P_start.y) * (P_end.y - P_start.y); //內積(馬路長平方)
    double t = ((P.x - P_start.x) * (P_end.x - P_start.x) + (P.y - P_start.y) * (P_end.y - P_start.y)) / length_squared; // t/馬路長平方
    return t;
}


// 接收封包處理
int PEDESTRIAN_plus_on_pedestrian_packet_rx(void* arg)
{
    PedestrianList* pedestrianlist = (PedestrianList*)arg;
    uint8_t current_SubPhaseID = get_current_phase();
    // if (current_SubPhaseID < 1 || current_SubPhaseID > 4) {
    //     enter_ped_phase_plus = false;
    //     return 0;
    // }

    if (pedestrianlist != NULL) {
        // if (!enter_ped_phase_plus) {
        //     enter_ped_phase_plus = true;
        //     pthread_mutex_lock(&matrix_mutex_plus);
        //     for (int i = 0; i < rows; i++) {
        //         for (int j = 0; j < cols; j++) matrix_plus[i][j] = 0;
        //     }
        //     pthread_mutex_unlock(&matrix_mutex_plus);
        //     for(int i=3; i<=8; i++) last_update_time_plus[i] = time(NULL);
        // }
        //fprintf(stderr, "[YOLO DEBUG] Camera %d YOLO : %d\n", pedestrianlist->camera_no, pedestrianlist->count);

        pthread_mutex_lock(&matrix_mutex_plus);
        int row_to_zero = (pedestrianlist->camera_no - 3) * 2;
        if(row_to_zero >= 0 && row_to_zero < rows) {
            for (int i = row_to_zero; i <= row_to_zero + 1; i++) {
                for (int j = 0; j < cols; j++) matrix_plus[i][j] = 0;
            }
        }


        waiting_area_plus[pedestrianlist->camera_no - 3] = 0;
        
        
        last_update_time_plus[pedestrianlist->camera_no] = time(NULL);
        Point P_start = {P_start_x_plus[pedestrianlist->camera_no], P_start_y_plus[pedestrianlist->camera_no]};
        Point P_end = {P_end_x_plus[pedestrianlist->camera_no], P_end_y_plus[pedestrianlist->camera_no]};
        
        if(pedestrianlist->count > 0) {
            for (int i = 0; i < pedestrianlist->count; i++) {
                Point P = { pedestrianlist->tab[i].cx, pedestrianlist->tab[i].cy};
                
                if (pedestrianlist->tab[i].waiting == 0) {
                    double projection_ratio = calculate_projection_on_road_plus(P, P_start, P_end);
                    if(projection_ratio > 0 && projection_ratio <= 1 ) {
                        pedestrianlist->tab[i].location = ceil(cols * projection_ratio) - 1;
                        if (pedestrianlist->tab[i].direction == 1 && cols - pedestrianlist->tab[i].location > 0) {
                            pedestrianlist->tab[i].location = cols - pedestrianlist->tab[i].location;
                        }

                        int row_no = -1;
                        if (pedestrianlist->camera_no >= 3 && pedestrianlist->camera_no <= 6) {
                            row_no = (pedestrianlist->camera_no - 3) * 2 + (pedestrianlist->tab[i].direction == 1 ? 0 : 1);
                        }

                        if(row_no >= 0 && row_no < rows && pedestrianlist->tab[i].location >= 0 && pedestrianlist->tab[i].location < cols) {
                            matrix_plus[row_no][pedestrianlist->tab[i].location] += 1;
                            // fprintf(stderr, "[VIPS+] Updated Matrix: Camera %d detects pedestrian at row %d, col %d (Projection Ratio: %.2f)\n", 
                            //         pedestrianlist->camera_no, row_no, pedestrianlist->tab[i].location, pedestrianlist->tab[i].location/11.0); 
                        }
                    }
                } else {
                    waiting_area_plus[pedestrianlist->camera_no - 3]++;
                }
            }
        }
        pthread_mutex_unlock(&matrix_mutex_plus);
    }
    return 0;
}

app_obj_t PEDESTRIAN_PLUS = {
    .name = "PEDESTRIAN_PLUS",
    .id = PEDESTRIAN_ID,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_pedestrian_packet_rx = PEDESTRIAN_plus_on_pedestrian_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = PEDESTRIAN_plus_on_registration,
    .next = NULL,
};

// WebSocket Callbacks
static int callback_websocket_plus(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        printf("Client connected (Plus)\n");
        pthread_mutex_lock(&ws_mutex_plus);
        if (client_count_plus < MAX_CLIENTS) {
            clients_plus[client_count_plus++] = wsi;
            lws_callback_on_writable(wsi);
        }
        pthread_mutex_unlock(&ws_mutex_plus);
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        char buf[LWS_PRE + MESSAGE_SIZE];
        pthread_mutex_lock(&ws_mutex_plus); 
        memset(&buf[LWS_PRE], 0, MESSAGE_SIZE);
        memcpy(&buf[LWS_PRE], message_buffer_plus, strlen(message_buffer_plus));
        pthread_mutex_unlock(&ws_mutex_plus);

        int write_result = lws_write(wsi, (unsigned char*)&buf[LWS_PRE], strlen(message_buffer_plus), LWS_WRITE_TEXT);
        if (write_result < 0) {
            printf("Failed to send message\n");
        }
        break;
    }
    case LWS_CALLBACK_CLOSED:
        printf("Client disconnected\n");
        pthread_mutex_lock(&ws_mutex_plus);
        for (int i = 0; i < client_count_plus; i++) {
            if (clients_plus[i] == wsi) {
                for (int j = i; j < client_count_plus - 1; j++) {
                    clients_plus[j] = clients_plus[j + 1];
                }
                clients_plus[--client_count_plus] = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&ws_mutex_plus);
        break;
    default:
        break;
    }
    return 0;
}

static void* ws_loop_plus(void* arg) {
    while (1) {
        lws_service(context_plus, 100);
    }
    return NULL;
}

void start_websocket_server_plus() {  
    struct lws_context_creation_info info;
    static struct lws_protocols protocols[] = {
        { "websocket-protocol", callback_websocket_plus, 0, 0 },
        { NULL, NULL, 0, 0 }
    };
    memset(&info, 0, sizeof(info));
    info.port = 8080; 
    info.protocols = protocols;

    context_plus = lws_create_context(&info);

    /*if (context_plus == NULL) {
        // 如果印出這行，代表 lws 內部失敗
        fprintf(stderr, "[DEBUG-B] lws_create_context 失敗！\n");
    } else {
        fprintf(stderr, "[DEBUG-B] lws_create_context 成功，位址：%p\n", (void*)context_plus);
    }*/
    pthread_t ws_tid;
    pthread_create(&ws_tid, NULL, ws_loop_plus, NULL);
    pthread_detach(ws_tid);
    printf("WebSocket server (Plus) started on ws://localhost:8080\n");
}

int PEDESTRIAN_plus_on_registration(void* arg)
{
    pthread_mutex_lock(&matrix_mutex_plus);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix_plus[i][j] = 0;
        }
    }
    for(int i= 0; i < waiting_rows; i++){
        waiting_area_plus[i] = 0;
    }
    pthread_mutex_unlock(&matrix_mutex_plus);

    start_sumo_receiver();

    start_websocket_server_plus();
    if (create_pedestrian_plus_timer() != 0) {
        printf("Failed to create timer\n");
        return -1;
    }

    if (start_pedestrian_plus_timer() != 0) {
        printf("Failed to start timer\n");
        return -1;
    }

    return 0;
}

static volatile int handler_running_plus = 0;
void pedestrian_plus_timer_handler(union sigval sv) {
    if (__sync_lock_test_and_set(&handler_running_plus, 1)) {
        return; 
    }

    
    static bool is_synchronized = false;
    static bool ready_to_send = false;
    static int ready_sent_plus = 0;
    static uint8_t last_SubPhaseID = 0;
    static uint8_t last_stepID = 0;
    uint8_t current_SubPhaseID = get_current_phase();
    uint8_t current_stepID = get_current_step();
    uint8_t current_phaseorder = get_PhaseOrder();
    uint8_t current_planID = get_plan_id();

    // fprintf(stderr, "[DEBUG] PlanID=%d, PhaseOrder=%d, SubPhaseID=%d, StepID=%d, ready_sent=%d, is_synchronized=%d, UR_plus=%.1f, PIC=%d, waiting_area_plus=%d %d %d %d\n", 
    //     current_planID, current_phaseorder, current_SubPhaseID, current_stepID, ready_sent_plus, is_synchronized, UR_plus, PIC_plus, waiting_area_plus[0], waiting_area_plus[1],waiting_area_plus[2], waiting_area_plus[3]);
    fprintf(stderr, "[DEBUG] PhaseOrder=%d, SubPhaseID=%d, StepID=%d, UR_plus=%.1f, PIC=%d, \n", current_phaseorder, current_SubPhaseID, current_stepID, UR_plus, PIC_plus );

    /*if (current_SubPhaseID == 2 && current_stepID == 5 && !ready_sent_plus) {
        pthread_mutex_lock(&ws_mutex_plus);
        strncpy(message_buffer_plus, "Red(CarPhase), 行人即將放行", MESSAGE_SIZE);

        if (context_plus != NULL) {
            for (int i = 0; i < client_count_plus; i++) {
                if(clients_plus[i]) lws_callback_on_writable(clients_plus[i]);
            }
            lws_cancel_service(context_plus); 
        }
        pthread_mutex_unlock(&ws_mutex_plus);
        ready_sent_plus = 1;
        printf("行人即將放行，請準備！(Plus)\n");
    }
    if (current_SubPhaseID != 2 || current_stepID != 5) {
        ready_sent_plus = 0;
    }*/
    if (!is_synchronized) { //演算法CS對齊號控器時相與步階
        //確保是從行綠早開開始
        if ((current_SubPhaseID == 2 && current_stepID == 5 ) || (current_SubPhaseID == 4 && current_stepID == 5)) {
            ready_to_send = true;
        } else if ((current_SubPhaseID != 1 && current_stepID != 1 ) && (current_SubPhaseID != 3 && current_stepID != 1)){
            tsc_switch_strategy();
            tsc_switch();
            sumo_switch_phase();//sumo
            tsc_5F4C();
        }

        if (((current_SubPhaseID == 1 && current_stepID == 1 ) || (current_SubPhaseID == 3 && current_stepID == 1)) && ready_to_send) {
            CS_plus = 1;
            is_synchronized = true;
            ready_to_send = false;
            direction_flag_plus = (current_SubPhaseID == 1) ? 0 : 1;
            fprintf(stderr, "[SYNC] Synchronized at SubPhaseID=%d, StepID=%d. Starting VIPS_plus from CS=1.\n", current_SubPhaseID, current_stepID);
        }
    }
    
    // 號誌與矩陣初始化
    pthread_mutex_lock(&matrix_mutex_plus);
    if(!time_flag_plus){
        second_plus = 0;
        time_flag_plus = true;
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) matrix_plus[i][j] = 0;
        }
        for(int i= 0; i < waiting_rows; i++){
            waiting_area_plus[i] = 0;
        }
        
        init_variable_plus(); 
    }
    pthread_mutex_unlock(&matrix_mutex_plus);
    second_plus++;
    // 攝影機斷線偵測
    time_t now = time(NULL);
    pthread_mutex_lock(&matrix_mutex_plus);
    for (int i = 3; i <= 6; i++) {
        if (difftime(now, last_update_time_plus[i]) >= 6) {
            int row_to_zero = (i - 3) * 2;
            for (int r = row_to_zero; r <= row_to_zero + 1; r++) {
                for (int c = 0; c < cols; c++) matrix_plus[r][c] = 0;
            }
            waiting_area_plus[i-3] = 0;
        }
    }
    pthread_mutex_unlock(&matrix_mutex_plus);

    if (is_synchronized) {
        //VIPS_plus 演算法 
        int pre_stage = CS_plus;
        runPedestrianSignalControl_plus(); 
        int post_stage = CS_plus;

    
        // 狀態切換時呼叫硬體控制
        if (pre_stage != post_stage) {
            tsc_switch_strategy();
            tsc_switch();
            // sumo_switch_phase();//sumo
            tsc_5F4C();
        }
    }

    // Debug 訊息
    memset(log_content_plus, 0, sizeof(log_content_plus));
    snprintf(log_content_plus, LOG_CONTENT_LEN, "\n[Sec %d] Matrix Plus (Stage %d):\n", second_plus, CS_plus);

    handler_running_plus = 0;
}

int create_pedestrian_plus_timer(void) {
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = pedestrian_plus_timer_handler;
    sev.sigev_value.sival_ptr = &pedestrian_timer_id_plus;

    if (timer_create(CLOCK_REALTIME, &sev, &pedestrian_timer_id_plus) == -1) {
        perror("timer_create plus");
        return -1;
    }
    return 0;
}

int start_pedestrian_plus_timer(void) {
    struct itimerspec its;
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 000000000;
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 000000000;

    if (timer_settime(pedestrian_timer_id_plus, 0, &its, NULL) == -1) {
        perror("timer_settime plus");
        return -1;
    }
    return 0;
}