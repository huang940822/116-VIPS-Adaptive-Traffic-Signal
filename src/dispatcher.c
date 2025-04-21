#include <pthread.h>
#include <stdio.h>

#include "com_packet_processing.h"
#include "dispatcher.h"
#include "log.h"
#include "msg_queue.h"
#include "server.h"
#include "threadpool.h"

LOG_USE_MODULE(CORE);

uint8_t cloud_com_id = 0;
uint8_t OBU_com_id = 0;
uint8_t Heartbeat_com_id = 0;
uint8_t AVI_com_id = 0;
threadpool_t *pool;
pthread_mutex_t lock;
// int f_flag = 0;
// which will continuously dequeue message objects form the message queue
void *dispatcher_handler()
{
    LOG_MSG_TRACE("Enter dispatcher handler");
    msg_queue_init();
    // pthread_mutex_init(&lock, NULL);
    int ret = 0;

    struct msg_obj *msg;
    // assert((pool = threadpool_create(THREAD, THREADQUEUE, 0)) != NULL);
    // LOG_MSG_INFO("Pool started with %d threads and queue size of %d", THREAD, THREADQUEUE);
    clock_t start_time, finish_time;
    int count = 0;
    double total_time = 0;
    for (;;) {
        msg = msg_queue_dequeue();
        //從queue中取出一個msg並判斷來自cloud, dsrc或是smart_avi
        if (msg->device_id == FROM_CLOUD) {
            // LOG_MSG_TRACE("cloud_rx_event");

            cloud_com_id = msg->handle_id;
            LOG_MSG_INFO("cloud_com_id in dispatcher is %d", cloud_com_id);
            // 處理從雲端接收到的封包，解析封包中的通用欄位，驗證這些欄位是否有效，然後根據封包中的服務 ID 調用相應的回調函式進行應用層負載的處理
            ret = cloud_packet_rx_event_handler(msg); // com_packet_processing.c
            if (ret < 0) {
                LOG_MSG_FATAL("invalid packet from cloud: %d", ret);
            }
        }
        if (msg->device_id == FROM_DSRC) {
            if (Is_Heartbeat(msg) == 1) {
                Heartbeat_com_id = msg->handle_id;
            } else {
                // if(f_flag < 2)
                OBU_com_id = msg->handle_id;
                // f_flag++;
                // 處理從 OBU 接收到的封包，解碼消息，解析封包中的記錄，根據車輛類型將記錄插入適當的結構，然後調用回調函式進行處理
                start_time = clock();
                ret = OBU_packet_rx_event_handler(msg);
                count++;
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                total_time += cost_time;
                if (count == 100)
                    LOG_MSG_TRACE("total time = %f", total_time);
                if (ret < 0) {
                    LOG_MSG_FATAL("invalid packet from OBU: %d", ret);
                }
            }
        }
        if (msg->device_id == FROM_SMART_AVI) {
            AVI_com_id = msg->handle_id;
            //從接收到的 Smart AVI 封包中提取障礙物list和時間戳訊息，然後調用相應的回調函式進行處理
            Smart_AVI_packet_rx_event_handler(msg);
        }
        free(msg);
    }
    assert(threadpool_destroy(pool, 0) == 0);
    LOG_MSG_FATAL("dispatcher thread exit");
}
