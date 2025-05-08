// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <libwebsockets.h>
// #include <unistd.h>

// #define MESSAGE_SIZE 128

// static struct lws* current_wsi = NULL;
// static char message_buffer[MESSAGE_SIZE] = "Red"; // 預設為 Red

// static int callback_websocket(struct lws* wsi, enum lws_callback_reasons reason,
//     void* user, void* in, size_t len) {
//     switch (reason) {
//     case LWS_CALLBACK_ESTABLISHED:
//         printf("Client connected\n");
//         current_wsi = wsi; // 儲存 wsi 指標以便後續傳訊息
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

// int main() {
//     struct lws_context_creation_info info;
//     struct lws_context* context;
//     struct lws_protocols protocols[] = {
//         {
//             "websocket-protocol",
//             callback_websocket,
//             0,
//             0
//         },
//         { NULL, NULL, 0, 0 }
//     };

//     memset(&info, 0, sizeof(info));
//     info.port = 8080;
//     info.protocols = protocols;

//     context = lws_create_context(&info);
//     if (context == NULL) {
//         printf("Failed to create WebSocket context\n");
//         return -1;
//     }

//     printf("WebSocket server started on ws://localhost:8080\nWating for client...\n");
//     // 等待客戶端連接
    

//     while (1) {
//         // 輸入控制訊號
        
//         if (current_wsi) {
//             lws_callback_on_writable(current_wsi); // 觸發發送訊號
//             int input = -1;
//             printf("輸入 0=Red, 1=Green, 2=GreenFlash：\n");
//             if (scanf("%d", &input) == 1) {
//                 if (input >= 0 && input <= 2) {
//                     const char* messages[] = { "Red", "Green", "GreenFlash" };
//                     strncpy(message_buffer, messages[input], MESSAGE_SIZE);
//                 }
//                 else {
//                     printf("無效輸入，請輸入 0, 1, 或 2\n");
//                 }
//             }
//             else {
//                 printf("無效輸入，請重新輸入\n");
//                 while (getchar() != '\n'); // 清除緩衝區
//             }
//         }
//         // 非阻塞事件處理
//         lws_service(context, 100); // 小 delay 避免卡住 UI
//     }

//     lws_context_destroy(context);
//     return 0;
// }