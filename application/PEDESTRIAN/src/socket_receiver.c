#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // Linux 標準函式
#include <sys/socket.h>  // Linux Socket
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // inet_ntoa
#include <pthread.h>
#include "Pedestrian.h"
#include "PEDESTRIAN_plus.h"
#include <netinet/tcp.h>

#define PORT 12345
#define HEADER_SIZE 24   
#define OBJ_SIZE 40      

extern int PEDESTRIAN_plus_on_pedestrian_packet_rx(void* arg);

extern int client_sock; // 確保能存取到與 Python 連線的 socket
int current_client_sock = -1;// 定義一個全域變數來存放目前連線的 Socket，供 tsc_switch 使用

void* socket_server_thread(void* arg) {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. 建立 Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Socket] 建立失敗");
        return NULL;
    }

    // 2. 設定允許重複使用 Port (避免重啟時顯示 Address already in use)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 3. 綁定與監聽
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[Socket] Bind 失敗");
        close(server_fd);
        return NULL;
    }

    listen(server_fd, 3);
    fprintf(stderr, "[Socket] Linux 接收層啟動成功，監聽 Port: %d\n", PORT);

    while ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
        fprintf(stderr, "[Socket] SUMO Adapter 已連線 (來自: %s)\n", inet_ntoa(address.sin_addr));

        int flag = 1;
        setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

        current_client_sock = client_sock;

        while (1) {
            char header_buf[HEADER_SIZE];
            // 接收 Header
            int n = recv(client_sock, header_buf, HEADER_SIZE, 0);
            if (n <= 0) break; 

            PedestrianList* list = (PedestrianList*)malloc(sizeof(PedestrianList));
            memset(list, 0, sizeof(PedestrianList));

            // 提取 count, camera_no, device_num
            memcpy(&list->count, header_buf + 8, 4);
            memcpy(&list->camera_no, header_buf + 12, 4);
            memcpy(&list->device_num, header_buf + 16, 4);

            if (list->count > 0) {
                list->tab = (Pedestrian*)malloc(OBJ_SIZE * list->count);
                int body_size = OBJ_SIZE * list->count;
                int received = 0;
                while (received < body_size) {
                    int r = recv(client_sock, ((char*)list->tab) + received, body_size - received, 0);
                    if (r <= 0) break;
                    received += r;
                }

                // --- 只有在有人時才印出二進位 Debug ---
                // unsigned char *ptr = (unsigned char*)list->tab; // 直接指向陣列開頭
                // fprintf(stderr, "[Debug Binary] ");
                // // 印出第一個行人的前 40 bytes
                // for(int j = 0; j < 40; j++) {
                //     fprintf(stderr, "%02x ", ptr[j]);
                // }
                // fprintf(stderr, "\n");
            } else {
                list->tab = NULL;
            }

            // --- 偵錯資訊開始 ---
            // fprintf(stderr, "\n==========================================\n");
            // fprintf(stderr, "[Socket RX] 收到相機 %d 的封包\n", list->camera_no);
            // fprintf(stderr, "[Socket RX] 行人數量: %u\n", list->count);

            // if (list->count > 0 && list->tab != NULL) {
            //     for (uint32_t i = 0; i < list->count; i++) {
            //         fprintf(stderr, "  行人 [%d]: ID=%u, waiting = %u, cx=%u, cy=%u, dir=%u, speed=%.2f\n",
            //                i, 
            //                list->tab[i].PERSON_ID, 
            //                list->tab[i].waiting, 
            //                list->tab[i].cx, 
            //                list->tab[i].cy, 
            //                list->tab[i].direction,
            //                list->tab[i].velocity);
            //     }
            // } else {
            //     fprintf(stderr, "  (目前無人)\n");
            // }
            // fprintf(stderr, "==========================================\n");
            // --- 偵錯資訊結束 ---

            // 呼叫 Middleware 處理
            PEDESTRIAN_plus_on_pedestrian_packet_rx((void*)list);

            if (list->tab) free(list->tab);
            free(list);
        }
        fprintf(stderr, "[Socket] 連線斷開，等待重新連線...\n");
        close(client_sock);
    }

    close(server_fd);
    return NULL;
}
//-----
extern int CS_plus;
extern int direction_flag_plus;

void sumo_switch_phase() {
    int sumo_index = 0;
    
    if (direction_flag_plus == 0) { // 南北向
        if (CS_plus == 1) sumo_index = 0;
        else if (CS_plus == 2 || CS_plus == 3) sumo_index = 1; 
        else if (CS_plus == 4) sumo_index = 2;                 
        else if (CS_plus == 5) sumo_index = 3;
        else if (CS_plus == 6) sumo_index = 4;
    } else { // 東西向
        if (CS_plus == 1) sumo_index = 5;
        else if (CS_plus == 2 || CS_plus == 3) sumo_index = 6; 
        else if (CS_plus == 4) sumo_index = 7;                 
        else if (CS_plus == 5) sumo_index = 8;
        else if (CS_plus == 6) sumo_index = 9;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "SET_PHASE:%d\n", sumo_index);
    
    if (current_client_sock >= 0) {
        int ret = send(current_client_sock, cmd, strlen(cmd), 0);
            if (ret < 0) {
                fprintf(stderr, "[ERROR] 發送失敗\n", errno, current_client_sock);
            } else {
                fprintf(stderr, "[SUCCESS] 成功送出\n", ret, current_client_sock);
            }
    }
}
//-----

void start_sumo_receiver() {
    pthread_t tid;
    pthread_create(&tid, NULL, socket_server_thread, NULL);
    pthread_detach(tid); 
}