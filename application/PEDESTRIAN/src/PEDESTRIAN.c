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


#include "PEDESTRIAN.h"
#include "PEDESTRIAN_timer_event.h"
#include "ObstacleList.h"
#include "Pedestrian.h"
#include "buffer.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "dispatcher.h"
#include "log.h"
#include "post_processing.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "traffic_signal_packet_tx.h"
#include "typedefine.h"
#define cols 10
#define rows 12 

#define CCL 10.0
#define LCL 8.0
#define SCL 5.0

#define G 11.0
#define GF 10.0
#define R 4.0
#define PT 25.0

#define MESSAGE_SIZE 128


/*演算法變數宣告*/
//LG, UG, LGF, UGF, LR, UR各號誌的上下限
double LG, UG, LGF, UGF, LR, UR;
//各號誌經過時間
double AccPedG = 0.0, AccPedGF = 0.0, AccPedR = 0.0;
//current state當前狀態
int CS = 0;
//通過累積人數, 這一秒的行人總數, 行人最大總數,
int PassPed = 0, PIC = 0;
double PedCount = 0.0;
//PP是否結束
bool PPend = false;

const char* TrafficLight[3] = { "GREEN", "GREEN FLASH", "RED" };

//確認第一次進入號誌時間
bool gStart_logged = false;
bool gfStart_logged = false;
bool rStart_logged = false;

time_t start_time, currentT, prev_read_time;
time_t gStart_time, gfStart_time, rStart_time;

timer_t PEDESTRIAN_Agent_timer_id;

bool enter_ped_phase = false;
time_t wait_start_time = 0;


typedef struct {
    double x;
    double y;
} Point;


int flag = 0;
double time_interval = 1;

/*要再去標點看實際長度*/
double road_length[9] = { 0,0,0,198.204,259.788,176.011,297.29,317.2,406.14 }; //馬路長度
int fusion[9] = { 0 };
// double P_start_x[9]={0,0,0,229,159,179,414,141,434};
// double P_start_y[9]={0,0,0,161,377,118,131,133,180};
// double P_end_x[9]={0,0,0,427,412,355,555,391,4};
// double P_end_y[9]={0,0,0,170,278,120,389,355,364};

double P_start_x[9] = { 0,0,0,215,165.5,375,180,148,41 };
double P_start_y[9] = { 0,0,0,156,117.5,118,353.5,131.5,348.5 };
double P_end_x[9] = { 0,0,0,443,353,147,407.5,367,413 };
double P_end_y[9] = { 0,0,0,159.5,120,115,272,360.5,185.5 };
//
/*3:PNorth
  4:NorthEast
  5:PSouth
  6:PEast
  7:PWestNortD
  8:PEastNorthD*/

int matrix[rows][cols] = { 0 }; //行人格位矩陣
int signalMatrix[3] = { 0,0,0 };

#define MAX_CLIENTS 10
struct lws_context* context = NULL;
static struct lws* clients[MAX_CLIENTS];
static int client_count = 0;
static char message_buffer[MESSAGE_SIZE] = "Red";   // 預設為 Red
static pthread_mutex_t ws_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t last_update_time[9];  // 記錄每個攝影機最後更新的時間
int second = 0;
timer_t pedestrian_timer_id;

char log_content[LOG_CONTENT_LEN + 1];// Prepare a log buffer
bool time_flag=false;



//假設馬路屬性road_num 馬路格位location 馬路投影量 projection 

double calculate_projection_on_road(Point P, Point P_start, Point P_end) {
    double length_squared = (P_end.x - P_start.x) * (P_end.x - P_start.x) + (P_end.y - P_start.y) * (P_end.y - P_start.y);
    double t = ((P.x - P_start.x) * (P_end.x - P_start.x) + (P.y - P_start.y) * (P_end.y - P_start.y)) / length_squared;
    return t;
}
// FILE *output;

app_obj_t PEDESTRIAN = {
    .name = "PEDESTRIAN",
    .id = PEDESTRIAN_ID,
    .priority = 3,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = NULL,
    .on_cloud_packet_tx = NULL,
    .on_camera_packet_rx = NULL,
    .on_pedestrian_packet_rx = PEDESTRIAN_on_pedestrian_packet_rx,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = PEDESTRIAN_on_registration,
    .next = NULL,
};

static int callback_websocket(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        printf("Client connected\n");
        if (client_count < MAX_CLIENTS) {
            clients[client_count++] = wsi;
            lws_callback_on_writable(wsi);
        }
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE: {
        char buf[LWS_PRE + MESSAGE_SIZE];
        memset(&buf[LWS_PRE], 0, MESSAGE_SIZE);
        memcpy(&buf[LWS_PRE], message_buffer, strlen(message_buffer));

        snprintf(log_content, sizeof(log_content),
            "[WS] Sent \"%s\"", message_buffer);
        log_file_write(log_content);

        int write_result = lws_write(wsi, (unsigned char*)&buf[LWS_PRE], strlen(message_buffer), LWS_WRITE_TEXT);
        if (write_result < 0) {
            printf("Failed to send message: %d\n", write_result);
        }
        else {
            printf("Sent to client: %s\n", message_buffer);
        }
        break;
    }

    case LWS_CALLBACK_CLOSED:
        printf("Client disconnected\n");
        for (int i = 0; i < client_count; i++) {
            if (clients[i] == wsi) {
                for (int j = i; j < client_count - 1; j++) {
                    clients[j] = clients[j + 1];
                }
                //client_count--;
                clients[--client_count] = NULL; ////新增的 把最後一位清掉
                break;
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

static void* ws_loop(void* arg) {
    while (1) {
        lws_service(context, 100);
    }
    return NULL;
}

void start_websocket_server() {
    struct lws_context_creation_info info;
    static struct lws_protocols protocols[] = {
        { "websocket-protocol", callback_websocket, 0, 0 },
        { NULL, NULL, 0, 0 }
    };

    memset(&info, 0, sizeof(info));
    info.port = 8080;
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (context == NULL) {
        printf("Failed to create WebSocket context\n");
        return;
    }
    pthread_t ws_tid;///
    pthread_create(&ws_tid, NULL, ws_loop, NULL);///
    pthread_detach(ws_tid);///
    printf("WebSocket server started on ws://localhost:8080\n");
}


int PEDESTRIAN_on_pedestrian_packet_rx(void* arg)
{

    memset(log_content, 0, sizeof(log_content));
    // Add a log message indicating the function was called
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\n PEDESTRIAN_on_pedestrian_packet_rx called.");
    log_file_write(log_content);
    // Cast the argument to PedestrianList
    PedestrianList* pedestrianlist = (PedestrianList*)arg;

    for (int i = 0; i < pedestrianlist->count; i++) {
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n Pedestrian %d: ID = %d, cx = %d, cy = %d, direction = %d",
            i,
            pedestrianlist->tab[i].PERSON_ID,
            pedestrianlist->tab[i].cx,
            pedestrianlist->tab[i].cy,
            pedestrianlist->tab[i].direction);
        log_file_write(log_content);
    }

    Point P_start;
    Point P_end;
    uint8_t current_SubPhaseID = get_current_phase();
    uint8_t current_stepID = get_current_step();



    if (current_SubPhaseID != 3) {
        enter_ped_phase = false;
    }


    // Ensure the pedestrianlist is not NULL
    if (current_SubPhaseID == 3 && pedestrianlist != NULL) { //行人專用時相且pedestrianlist不=null

        if (!enter_ped_phase) {
            enter_ped_phase = true;
            for (int i = 0;i < rows;i++) {
                for (int j = 0;j < cols;j++)matrix[i][j] = 0;
            }

            for(int i=3;i<=8;i++)last_update_time[i] = time(NULL);
            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\nenter subphase3");
            log_file_write(log_content);
            //wait_start_time = time(NULL);
        }
        // time_t now = time(NULL);
        //  double elapsed = difftime(now, wait_start_time);
        //     snprintf(log_content + strlen(log_content), 
        //          LOG_CONTENT_LEN - strlen(log_content), 
        //          "\n elapsed time=%.0f", elapsed);
        //         log_file_write(log_content);

        // if(elapsed>=3){//有camera壞掉

        //     wait_start_time = time(NULL);
        //     for(int i = 3;i < 9;i++){
        //         if(fusion[i] == 0) {
        //             fusion[i]=2;
        //             snprintf(log_content + strlen(log_content), 
        //             LOG_CONTENT_LEN - strlen(log_content), 
        //             "\n Camera_No: %d is broken ,elapsed time=%.0f", i, elapsed);
        //             log_file_write(log_content);
        //         }
        //     }
        //      memset(log_content, 0, sizeof(log_content));
        //     snprintf(log_content + strlen(log_content), 
        //             LOG_CONTENT_LEN - strlen(log_content), 

        //             "\n fusion" );
        //      snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),"\n" );
        //         for(int j=3;j<9;j++){
        //             snprintf(log_content + strlen(log_content), 
        //             LOG_CONTENT_LEN - strlen(log_content), " %d ",fusion[j] );
        //         }
        //         snprintf(log_content + strlen(log_content), 
        //             LOG_CONTENT_LEN - strlen(log_content), 

        //             "\n fusion end" );
        //     log_file_write(log_content);
        // }
        // Log the number of pedestrians
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n Number of pedestrians: %d \n Camera_No: %d", pedestrianlist->count, pedestrianlist->camera_no);
        log_file_write(log_content);


        //  if(fusion[pedestrianlist->camera_no]>=1){
                //將此馬路兩方向清零重填

        int row_to_zero = (pedestrianlist->camera_no - 3) * 2;
        for (int i = row_to_zero;i <= row_to_zero + 1;i++) {
            for (int j = 0;j < cols;j++)
                matrix[i][j] = 0;
        }
        //更新收到封包的時間
        last_update_time[pedestrianlist->camera_no] = time(NULL);
        
        time_t now = time(NULL);
        //長時間未收到封包
        for (int i = 3; i <= 8; i++) {
            if (difftime(now, last_update_time[i]) >= 6) {
                // 該攝影機超過3秒沒更新，將對應matrix歸零
                row_to_zero = (i - 3) * 2;
                for (int r = row_to_zero; r <= row_to_zero + 1; r++) {
                    for (int c = 0; c < cols; c++) {
                        matrix[r][c] = 0;
                    }
                   
                }
                 last_update_time[i] = time(NULL);
                 memset(log_content, 0, sizeof(log_content));
                snprintf(log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "\n Camera_No: %d is broken", i);
                log_file_write(log_content);
            }
        }



        // }
                // fusion[pedestrianlist->camera_no]=1;
        P_start.x = P_start_x[pedestrianlist->camera_no];
        P_start.y = P_start_y[pedestrianlist->camera_no];
        P_end.x = P_end_x[pedestrianlist->camera_no];
        P_end.y = P_end_y[pedestrianlist->camera_no];


        if(pedestrianlist->count==0) return 0;


        // Loop through each pedestrian and log their information
        for (int i = 0; i < pedestrianlist->count; i++) {
            //計算行人所在格位(location)
            Point P = { pedestrianlist->tab[i].cx, pedestrianlist->tab[i].cy };
            double projection_ratio = calculate_projection_on_road(P, P_start, P_end);

            if(projection_ratio > 0 && projection_ratio <= 1) {
                pedestrianlist->tab[i].location = ceil(cols * projection_ratio) - 1;
                if (pedestrianlist->tab[i].direction == 0) {
                    pedestrianlist->tab[i].location = cols - pedestrianlist->tab[i].location - 1;
                }

                memset(log_content, 0, sizeof(log_content));
                snprintf(log_content + strlen(log_content),
                    LOG_CONTENT_LEN - strlen(log_content),
                    "\n Pedestrian %d: ID = %d, cx = %d, cy = %d, direction= %d,location= %d",
                    i,
                    pedestrianlist->tab[i].PERSON_ID,
                    pedestrianlist->tab[i].cx,
                    pedestrianlist->tab[i].cy,
                    pedestrianlist->tab[i].direction,
                    pedestrianlist->tab[i].location
                );
                log_file_write(log_content);

                //計算行人所在馬路
                int row_no;
                if (pedestrianlist->camera_no >= 3 && pedestrianlist->camera_no <= 8) {
                    row_no = (pedestrianlist->camera_no - 3) * 2 + (pedestrianlist->tab[i].direction == 0 ? 1 : 0);
                }

                //填入行人格位矩陣
                matrix[row_no][pedestrianlist->tab[i].location] += 1;
            }
            
        }





        // if((fusion[3]==1||fusion[3]==2) &&
        //     (fusion[4]==1||fusion[4]==2) && 
        //     (fusion[5]==1||fusion[5]==2) && 
        //     // (fusion[6]==1||fusion[6]==2) &&
        //     (fusion[7]==1||fusion[7]==2) && 
        //     (fusion[8]==1||fusion[8]==2)
        //     ) {
            // memset(log_content, 0, sizeof(log_content));
            // snprintf(log_content + strlen(log_content), 
            //         LOG_CONTENT_LEN - strlen(log_content), 

            //         "\n Pedestrian matrix:" );
            // for(int i=0;i<rows;i++){
            //     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),"\n" );
            //     for(int j=0;j<cols;j++){
            //         snprintf(log_content + strlen(log_content), 
            //         LOG_CONTENT_LEN - strlen(log_content), "%d ",matrix[i][j] );
            //     }
            // }
            // snprintf(log_content + strlen(log_content), 
            //         LOG_CONTENT_LEN - strlen(log_content), 

            //         "\n Pedestrian matrix end" );
        //log_file_write(log_content);
        /////call 演算法


        // current_SubPhaseID=get_current_phase();
        // current_stepID=get_current_step();


        // memset(log_content, 0, sizeof(log_content));
        // snprintf(log_content + strlen(log_content), 
        //                 LOG_CONTENT_LEN - strlen(log_content), 
        //                 "\n current SubPhaseID 3 ,current StepID=%u ",current_stepID);
        //     log_file_write(log_content);


        //矩陣、fusion歸零
        // for(int i = 0;i < rows;i++){
        //     for(int j = 0;j < cols;j++){
        //         matrix[i][j] = 0;
        //     }
        // }
        // for(int i = 3;i < 9;i++){
        //     if(fusion[i]!=2) fusion[i] = 0;
        // }

    // }


    }
    else {
        // Log an error message if pedestrianlist is NULL
        snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n Error: PedestrianList is NULL.");
        log_file_write(log_content);
    }

}

// int PEDESTRIAN_on_pedestrian_packet_rx(void *arg)
// {
//     // Prepare a log buffer
//     char log_content[LOG_CONTENT_LEN + 1];
//     memset(log_content, 0, sizeof(log_content));

//     // Add a log message indicating the function was called
//     snprintf(log_content + strlen(log_content), 
//              LOG_CONTENT_LEN - strlen(log_content), 
//              "\n PEDESTRIAN_on_pedestrian_packet_rx called.");
//     log_file_write(log_content);
//     // Cast the argument to PedestrianList
//     PedestrianList *pedestrianlist = (PedestrianList *)arg;
//     for (int i = 0; i < pedestrianlist->count; i++) {
//             memset(log_content, 0, sizeof(log_content));
//             snprintf(log_content + strlen(log_content), 
//                      LOG_CONTENT_LEN - strlen(log_content), 
//                      "\n Pedestrian %d: ID = %d, cx = %d, cy = %d, direction = %d", 
//                      i, 
//                      pedestrianlist->tab[i].PERSON_ID, 
//                      pedestrianlist->tab[i].cx, 
//                      pedestrianlist->tab[i].cy,
//                      pedestrianlist->tab[i].direction);
//             log_file_write(log_content);
//     }

// }

int PEDESTRIAN_on_registration(void* arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), \
        LOG_CONTENT_LEN - strlen(log_content), \
        "\n Pedestrian_on_Registration ");
    log_file_write(log_content);

    for (int i = 0;i < rows;i++) {
        for (int j = 0;j < cols;j++) {
            matrix[i][j] = 0;
        }
    }

    if (create_pedestrian_timer() != 0) {
        printf("Failed to create timer\n");
        return -1;
    }

    if (start_pedestrian_timer() != 0) {
        printf("Failed to start timer\n");
        return -1;
    }

    start_websocket_server();

    //我先註解 測試看看create_timer(&PEDESTRIAN_Agent_timer_id, NULL, PEDESTRIAN_Agent_timer_handler);
    //我先註解 測試看看set_timer(PEDESTRIAN_Agent_timer_id, 0, 100000000, 4, 0); // 程式開始後 4 秒，每 0.1 秒觸發一次 PEDESTRIAN_Agent_timer_handler
    //create_timer(&PEDESTRIAN_Agent_timer_id, NULL, PEDESTRIAN_Agent_timer_handler);
    //set_timer(PEDESTRIAN_Agent_timer_id, 0, 100000000, 4, 0); // 程式開始後 4 秒，每 0.1 秒觸發一次 PEDESTRIAN_Agent_timer_handler

    // while(1){
    //     memset(log_content, 0, sizeof(log_content));
    //     snprintf(log_content + strlen(log_content), 
    //              LOG_CONTENT_LEN - strlen(log_content), 
    //              "\n Velocity_average %f", velocity_average);
    //     log_file_write(log_content);
    //     sleep(1);
    // }
}

static volatile int handler_running = 0;

void pedestrian_timer_handler(union sigval sv) {
    if (__sync_lock_test_and_set(&handler_running, 1)) {
        // 上次還沒跑完，這次直接跳過
        return;
    }

    //新加////////////////////////////////////////////
    static int ready_sent = 0; // 保證只推一次 Ready

    uint8_t current_SubPhaseID = get_current_phase();
    uint8_t current_stepID = get_current_step();
    printf("[DEBUG] SubPhaseID=%d, StepID=%d, ready_sent=%d\n", current_SubPhaseID, current_stepID, ready_sent);


    if (current_SubPhaseID == 2 && current_stepID == 5 && !ready_sent) {
        pthread_mutex_lock(&ws_mutex);
        strncpy(message_buffer, "Red(CarPhase), 行人即將放行", MESSAGE_SIZE);
        // printf("Sent to client: %s\n", message_buffer);
        for (int i = 0; i < client_count; i++) {
            lws_callback_on_writable(clients[i]);
        }
        lws_cancel_service(context);
        pthread_mutex_unlock(&ws_mutex);
        ready_sent = 1;
        printf("行人即將放行，請準備！\n");
    }
    // 狀態一旦離開就 reset
    if (current_SubPhaseID != 2 || current_stepID != 5) {
        ready_sent = 0;
    }
    ////////////////////////////////////////////////////

    if (get_current_phase() != 3) {
        time_flag=false;
        handler_running = 0;
        if(get_current_phase()==1){//把subphase==1的狀態切掉以減少demo等待時間
            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n cut1 current SubPhaseID=%u ,current StepID=%u", get_current_phase(), get_current_step());
            log_file_write(log_content);


            tsc_switch_strategy();// 策略模式
            tsc_switch();
            tsc_5F4C();
        }
        return;
    }
    if(!time_flag){
        second=0;
        time_flag=true;
        for (int i = 0;i < rows;i++) {
                for (int j = 0;j < cols;j++)matrix[i][j] = 0;
            }
        /*演算法變數重置*/
        PPend = false;
        gStart_logged = false;
        gfStart_logged = false;
        rStart_logged = false;
        init_variable();
    }
    second++;
    pthread_mutex_t matrix_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&matrix_mutex);
    int pre_subphase = CS;
    runPedestrianSignalControl();
    int post_subphase = CS;
    pthread_mutex_unlock(&matrix_mutex);

    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\nped timer success");
    log_file_write(log_content);

    if (pre_subphase != post_subphase) {
        // //call API
        uint8_t current_SubPhaseID = get_current_phase();
        uint8_t current_stepID = get_current_step();

        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n current SubPhaseID=%u ,current StepID=%u ,prephase=%u , postphase=%u", current_SubPhaseID, current_stepID, pre_subphase, post_subphase);
        log_file_write(log_content);


        tsc_switch_strategy();// 策略模式
        tsc_switch();
        tsc_5F4C();

        

    }
    if (PPend == true) {
        //enter_ped_phase=false;
        
        
        tsc_switch_strategy();// 策略模式
        tsc_switch();
        tsc_5F4C();
        memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content),
            LOG_CONTENT_LEN - strlen(log_content),
            "\n change to ATSC");
        log_file_write(log_content);


    }
    memset(log_content, 0, sizeof(log_content));
        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\nPedestrian matrix %d th sec:\n",second);

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%d ", matrix[i][j]);
            }
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "\n");
        }

        snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "Pedestrian matrix end\n");

       
        log_file_write(log_content);
    handler_running = 0;
}
int create_pedestrian_timer() {
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = pedestrian_timer_handler;
    sev.sigev_value.sival_ptr = &pedestrian_timer_id;

    if (timer_create(CLOCK_REALTIME, &sev, &pedestrian_timer_id) == -1) {
        perror("timer_create");
        return -1;
    }
    return 0;
}
int start_pedestrian_timer() {
    struct itimerspec its;
    its.it_value.tv_sec = 1;  // 第一次延遲 1 秒
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1;  // 每 1 秒觸發
    its.it_interval.tv_nsec = 0;



    if (timer_settime(pedestrian_timer_id, 0, &its, NULL) == -1) {
        perror("timer_settime");
        return -1;
    }
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\nstart timer success");
    log_file_write(log_content);
    return 0;
}


/* 演算法function */
//計算各號誌時間上下限&變數初始化
void init_variable() {
    LG = (G + GF) * 0.3;
    UG = (G + GF) * 0.4;
    LGF = (G + GF) * 0.5;
    UGF = (G + GF) * 0.6;
    LR = 3;
    UR = LR + 0.1 * PT;
    
    AccPedG = 0.0;
    AccPedGF = 0.0;
    AccPedR = 0.0;
    CS = 0;
    PassPed = 0;
    PIC = 0;
    PedCount = 0.0;
}

//計算行人最大值
void calculateMaxPed() {
    PIC = 0;
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 10; j++) {
            PIC += matrix[i][j];
        }
    }
    if (PIC > PedCount) {
        PedCount = PIC;
    }
}

//累積通過人數
void calculatePassPed() {
    for (int i = 0; i < 12; i++) {
        PassPed += matrix[i][9];
    }
}

void alg1() {
    AccPedG = difftime(time(NULL), gStart_time);

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\nGreen Elasped Time: %.0f", AccPedG);
    log_file_write(log_content);

    bool changeState = false;
    if (AccPedG >= LG) {
        //斜邊斑馬線(30%)
        for (int i = 8; i <= 11 && !changeState; i++) {
            for (int j = 3; j < 10; j++) {
                if (matrix[i][j] > 0) {
                    changeState = true;
                    CS = 1;
                    break;
                }
            }
        }
        //短邊斑馬線(70%)
        for (int i = 0; i <= 3 && !changeState; i++) {
            for (int j = 7; j < 10; j++) {
                if (matrix[i][j] > 0) {
                    changeState = true;
                    CS = 1;
                    break;
                }
            }
        }
        //長邊斑馬線(50%)
        for (int i = 4; i <= 7 && !changeState; i++) {
            for (int j = 5; j < 10; j++) {
                if (matrix[i][j] > 0) {
                    changeState = true;
                    CS = 1;
                    break;
                }
            }
        }
    }
    if (AccPedG >= UG) {
        CS = 1;
    }
}

void alg2() {
    AccPedGF = difftime(time(NULL), gfStart_time);

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\nGreenFlash Elasped Time: %.0f", AccPedGF);
    log_file_write(log_content);

    if (AccPedGF >= LGF) {
        if (PassPed >= (PedCount * 0.8) || AccPedGF >= UGF) {
            CS = 2;
        }
    }
}

void alg3() {
    AccPedR = difftime(time(NULL), rStart_time);

    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
        LOG_CONTENT_LEN - strlen(log_content),
        "\nRed Elasped Time: %.0f", AccPedR);
    log_file_write(log_content);

    if (AccPedR >= LR) {
        if (PIC == 0 || AccPedR >= UR) {
            PPend = true;
        }
    }
}

/*演算法main function*/
int runPedestrianSignalControl() {
    //init_variable();  //可以移出去只要計算一次不用每次都進行計算

    //printTLState();
    //start_time = time(NULL) - 1;
    //prev_read_time = start_time;

    //printf("---------- while ----------\n");


    

    if (!PPend) {
        //currentT = time(NULL);

        //if (difftime(currentT, prev_read_time) >= 1.0) {
            //prev_read_time = currentT;

            //這裡原本是在讀測資，所以應該要用來抓Ped和TL
        calculateMaxPed();
        calculatePassPed();

        switch (CS) {
        case 0:  //綠燈情況
            if (!gStart_logged) {
                gStart_time = time(NULL) - 1;
                gStart_logged = true;
            }
            alg1();
            break;

        case 1:  //綠閃情況
            if (!gfStart_logged) {
                gfStart_time = time(NULL) - 1;
                gfStart_logged = true;
            }
            alg2();
            break;

        case 2:  //紅燈情況
            if (!rStart_logged) {
                rStart_time = time(NULL) - 1;
                rStart_logged = true;
            }
            alg3();
            break;

        default:
            //printf("Warning: Unrecognized CS value\n");
            break;
        }

        signalMatrix[2] = CS;

        // 設定訊息內容
        pthread_mutex_lock(&ws_mutex);
        switch (CS) {
        case 0:
            strncpy(message_buffer, "Green", MESSAGE_SIZE);
            break;
        case 1:
            strncpy(message_buffer, "GreenFlash", MESSAGE_SIZE);
            break;
        case 2:
            if(PPend) {
                strncpy(message_buffer, "Red(CarPhase)", MESSAGE_SIZE);
                printf("Red(CarPhase)\n");
            }
            else {
                strncpy(message_buffer, "Red", MESSAGE_SIZE);
            }
            break;
        default:
            break;
        }

        // 廣播給所有連線中的 client
        for (int i = 0; i < client_count; i++) {
            lws_callback_on_writable(clients[i]);
        }
        lws_cancel_service(context);  /* ← 喚醒 service loop */
        pthread_mutex_unlock(&ws_mutex);
        //printTLState();

        //double elapsed = difftime(currentT, start_time);

        //printf("Elapsed Time: %.0f sec\n", elapsed);
        //printf("--------------------\n");
    //}
    }

    //printf("Enter ATSC...!\n");
    return 0;
}