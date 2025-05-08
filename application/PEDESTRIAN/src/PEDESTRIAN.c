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
//#include <libwebsockets.h>



#include "PEDESTRIAN.h"
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
//通過累積人數, 行人最大總數, 這一秒的行人總數
int PassPed = 0, PedCount = 0, PIC = 0;
//PP是否結束
bool PPend = false;

const char *TrafficLight[3] = {"GREEN", "GREEN FLASH", "RED"};

//確認第一次進入號誌時間
bool gStart_logged = false;
bool gfStart_logged = false;
bool rStart_logged = false;

time_t start_time, currentT, prev_read_time;
time_t gStart_time, gfStart_time, rStart_time;



typedef struct {
    double x;
    double y;
} Point;


int flag = 0;
double time_interval = 1;

/*要再去標點看實際長度*/
double road_length[9] = {0,0,0,198.204,259.788,176.011,297.29,317.2,406.14}; //馬路長度
int fusion[9] = {0};
// double P_start_x[9]={0,0,0,229,159,179,414,141,434};
// double P_start_y[9]={0,0,0,161,377,118,131,133,180};
// double P_end_x[9]={0,0,0,427,412,355,555,391,4};
// double P_end_y[9]={0,0,0,170,278,120,389,355,364};

double P_start_x[9]={0,0,0,443,412,375,557,381,41};
 double P_start_y[9]={0,0,0,161,278,118,391.5,354,348.5};
 double P_end_x[9]={0,0,0,210,159,147,416.5,145.5,413};
 double P_end_y[9]={0,0,0,156,377,115,129.5,141.5,185.5};
 //
/*3:PNorth
  4:NorthEast
  5:PSouth
  6:PEast
  7:PWestNortD
  8:PEastNorthD*/

int matrix[rows][cols] = {0}; //行人格位矩陣
int signalMatrix[3]={0,0,0};

struct lws_context* context = NULL;
static struct lws* current_wsi = NULL;
static char message_buffer[MESSAGE_SIZE] = "Red";   // 預設為 Red

int second=0;


//假設馬路屬性road_num 馬路格位location 馬路投影量 projection 

double calculate_projection_on_road(Point P, Point P_start, Point P_end) {
    double length_squared = (P_end.x - P_start.x)*(P_end.x - P_start.x) + (P_end.y - P_start.y)*(P_end.y - P_start.y);
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

// static int callback_websocket(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
//     switch (reason) {
//     case LWS_CALLBACK_ESTABLISHED:
//         printf("Client connected\n");
//         current_wsi = wsi;
//         break;
//     case LWS_CALLBACK_SERVER_WRITEABLE: {
//         char buf[LWS_PRE + MESSAGE_SIZE];
//         memset(&buf[LWS_PRE], 0, MESSAGE_SIZE);
//         memcpy(&buf[LWS_PRE], message_buffer, strlen(message_buffer));

//         int write_result = lws_write(wsi, (unsigned char*)&buf[LWS_PRE], strlen(message_buffer), LWS_WRITE_TEXT);
//         if (write_result < 0) {
//             printf("Failed to send message: %d\n", write_result);
//         }
//         else {
//             printf("Sent: %s\n", message_buffer);
//         }
//         break;
//     }
//     case LWS_CALLBACK_CLOSED:
//         printf("Client disconnected\n");
//         current_wsi = NULL;
//         break;
//     default:
//         break;
//     }
//     return 0;
// }

// void start_websocket_server() {
//     struct lws_context_creation_info info;
//     static struct lws_protocols protocols[] = {
//         { "websocket-protocol", callback_websocket, 0, 0 },
//         { NULL, NULL, 0, 0 }
//     };

//     memset(&info, 0, sizeof(info));
//     info.port = 8080;
//     info.protocols = protocols;

//     context = lws_create_context(&info);
//     if (context == NULL) {
//         printf("Failed to create WebSocket context\n");
//         return;
//     }

//     printf("WebSocket server started on ws://localhost:8080\n");
// }


int PEDESTRIAN_on_pedestrian_packet_rx(void *arg)
{
    // Prepare a log buffer
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    // Add a log message indicating the function was called
    snprintf(log_content + strlen(log_content), 
             LOG_CONTENT_LEN - strlen(log_content), 
             "\n PEDESTRIAN_on_pedestrian_packet_rx called.");
    log_file_write(log_content);
    // Cast the argument to PedestrianList
    PedestrianList *pedestrianlist = (PedestrianList *)arg;

    Point P_start;
    Point P_end;

    //得到號誌的API
    uint8_t current_SubPhaseID=get_current_phase();
    uint8_t current_stepID=get_current_step();
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), 
                 LOG_CONTENT_LEN - strlen(log_content), 
                 "\n current PhaseID=%u ,current StepID=%u ",current_SubPhaseID,current_stepID);
        log_file_write(log_content);


    // Ensure the pedestrianlist is not NULL
    if (/*現在不是行人時區 &&*/ pedestrianlist != NULL) { 
        // Log the number of pedestrians
        snprintf(log_content + strlen(log_content), 
                 LOG_CONTENT_LEN - strlen(log_content), 
                 "\n Number of pedestrians: %d \n Camera_No: %d", pedestrianlist->count, pedestrianlist->camera_no);
        log_file_write(log_content);
        
            //重複傳的封包的封包丟棄
            if(fusion[pedestrianlist->camera_no]==1){
                return 0;
            }

                fusion[pedestrianlist->camera_no]=1;
                P_start.x = P_start_x[pedestrianlist->camera_no];
                P_start.y = P_start_y[pedestrianlist->camera_no];
                P_end.x = P_end_x[pedestrianlist->camera_no];
                P_end.y = P_end_y[pedestrianlist->camera_no];
                
                

        
        
        // Loop through each pedestrian and log their information
        for (int i = 0; i < pedestrianlist->count; i++) {
            
               
            //計算行人所在格位(location)
            Point P = {pedestrianlist->tab[i].cx, pedestrianlist->tab[i].cy};
            double projection_ratio = calculate_projection_on_road(P, P_start, P_end) ;
            pedestrianlist->tab[i].location = ceil(cols * projection_ratio)-1; 
            if(pedestrianlist->tab[i].direction==0){
                pedestrianlist->tab[i].location=cols-pedestrianlist->tab[i].location-1;
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
            matrix[row_no][pedestrianlist->tab[i].location]+=1;
        }

       
        
        //get signal
        
        if(fusion[3] && fusion[4] && fusion[5] && /*fusion[6] &&*/ fusion[7] && fusion[8]) {
            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content), 
                    LOG_CONTENT_LEN - strlen(log_content), 

                    "\n Pedestrian matrix:" );
            for(int i=0;i<rows;i++){
                snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content),"\n" );
                for(int j=0;j<cols;j++){
                    snprintf(log_content + strlen(log_content), 
                    LOG_CONTENT_LEN - strlen(log_content), "%d ",matrix[i][j] );
                }
            }
            snprintf(log_content + strlen(log_content), 
                    LOG_CONTENT_LEN - strlen(log_content), 

                    "\n Pedestrian matrix end" );
            log_file_write(log_content);
            /////call 演算法
            

            runPedestrianSignalControl();

            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content), 
                         LOG_CONTENT_LEN - strlen(log_content), 
                         "\n %d th second signalMatrix[2]=%s ",++second,TrafficLight[signalMatrix[2]]);
                log_file_write(log_content);
            // //call API
            // tsc_pretime();  // 切換為即時控制狀態
            // sleep(1);       // 建議給 0.5~1 秒緩衝讓控制器進入 pretiming 狀態
            
            // tsc_switch();
            //矩陣、fusion歸零
            for(int i = 0;i < rows;i++){
                for(int j = 0;j < cols;j++){
                    matrix[i][j] = 0;
                }
            }
            for(int i = 3;i < 9;i++){
                fusion[i] = 0;
            }
        }
        
        
    //////////flag call 演算法


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

int PEDESTRIAN_on_registration(void *arg)
{   
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),   \
            LOG_CONTENT_LEN - strlen(log_content), \
            "\n Pedestrian_on_Registration ");
    log_file_write(log_content);

    tsc_pretime();//切換控制策略
    sleep(2);
    //測試tsc_switch()
    uint8_t current_SubPhaseID=get_current_phase();
    uint8_t current_stepID=get_current_step();
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), 
                 LOG_CONTENT_LEN - strlen(log_content), 
                 "\n current SubPhaseID=%u ,current StepID=%u ",current_SubPhaseID,current_stepID);
        log_file_write(log_content);

      tsc_switch();//轉換號誌
      sleep(1);
      tsc_5F4C();//query當前號誌
      sleep(1);
       current_SubPhaseID=get_current_phase();
       current_stepID=get_current_step();
     
       memset(log_content, 0, sizeof(log_content));
       snprintf(log_content + strlen(log_content), 
                    LOG_CONTENT_LEN - strlen(log_content), 
                    "\n current SubPhaseID=%u ,current StepID=%u ",current_SubPhaseID,current_stepID);
           log_file_write(log_content);

        tsc_switch();
        sleep(1);
        tsc_5F4C();//query當前號誌
        sleep(1);
       current_SubPhaseID=get_current_phase();
       current_stepID=get_current_step();
     
       memset(log_content, 0, sizeof(log_content));
       snprintf(log_content + strlen(log_content), 
                    LOG_CONTENT_LEN - strlen(log_content), 
                    "\n current SubPhaseID=%u ,current StepID=%u ",current_SubPhaseID,current_stepID);
           log_file_write(log_content);
    
    //start_websocket_server();

    // while(1){
    //     memset(log_content, 0, sizeof(log_content));
    //     snprintf(log_content + strlen(log_content), 
    //              LOG_CONTENT_LEN - strlen(log_content), 
    //              "\n Velocity_average %f", velocity_average);
    //     log_file_write(log_content);
    //     sleep(1);
    // }
}

/* 演算法function */
//計算各號誌長度
void init_variable() {
    LG = (G + GF) * 0.3;
    UG = (G + GF) * 0.4;
    LGF = (G + GF) * 0.5;
    UGF = (G + GF) * 0.6;
    LR = 3;
    UR = LR + 0.1 * PT;
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
        //長邊斑馬線(50%)
        for (int i = 0; i <= 3 && !changeState; i++) {
            for (int j = 5; j < 10; j++) {
                if (matrix[i][j] > 0) {
                    changeState = true;
                    CS = 1;
                    break;
                }
            }
        }
        //短邊斑馬線(70%)
        for (int i = 4; i <= 7 && !changeState; i++) {
            for (int j = 7; j < 10; j++) {
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
    if (AccPedGF >= LGF) {
        if (PassPed >= (PedCount * 0.8) || AccPedGF >= UGF) {
            CS = 2;
        }
    }
}

void alg3() {
    AccPedR = difftime(time(NULL), rStart_time);
    if (AccPedR >= LR) {
        if (PIC == 0 || AccPedR >= UR) {
            PPend = true;
        }
    }
}

/*演算法main function*/
int runPedestrianSignalControl() {
    
    init_variable();  //可以移出去只要計算一次不用每次都進行計算

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

            if (current_wsi != NULL) {
                switch (CS) {
                case 0: // 綠燈
                    strncpy(message_buffer, "Green", MESSAGE_SIZE);
                    break;
                case 1: // 綠閃
                    strncpy(message_buffer, "GreenFlash", MESSAGE_SIZE);
                    break;
                case 2: // 紅燈
                    strncpy(message_buffer, "Red", MESSAGE_SIZE);
                    break;
                default:
                    break;
                }
                //lws_callback_on_writable(current_wsi);
            }

            //printTLState();

            //double elapsed = difftime(currentT, start_time);

            //printf("Elapsed Time: %.0f sec\n", elapsed);
            //printf("--------------------\n");
        //}
    }

    //printf("Enter ATSC...!\n");
    return 0;
}