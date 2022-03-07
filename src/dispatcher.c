#include <pthread.h>
#include <stdio.h>

#include "com_packet_processing.h"
#include "dispatcher.h"
#include "log.h"
#include "msg_queue.h"
#include "server.h"
#include "threadpool.h"

uint8_t cloud_com_id;
uint8_t OBU_com_id;
uint8_t Heartbeat_com_id;
uint8_t AVI_com_id;
threadpool_t *pool;
pthread_mutex_t lock;
int f_flag = 0;
// which will continuously dequeue message objects form the message queue
void *dispatcher_handler()
{
    msg_queue_init();
    pthread_mutex_init(&lock, NULL);
    int ret = 0;
    char log_content[LOG_CONTENT_LEN + 1];

    struct msg_obj *msg;
    assert((pool = threadpool_create(THREAD, QUEUE, 0)) != NULL);
    fprintf(stderr,
            "Pool started with %d threads and "
            "queue size of %d\n",
            THREAD, QUEUE);

    for (;;) {
        memset(log_content, 0, sizeof(log_content));
        msg = msg_queue_dequeue();

        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "dispatcher: MSG(%d)",
                 msg->device_id);
        log_file_write(log_content);
        if (msg->device_id == FROM_CLOUD) {
            // printf("cloud_rx_event\n");

            cloud_com_id = msg->handle_id;
            memset(log_content, 0, sizeof(log_content));
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "cloud_com_id in dispatcher is %d", cloud_com_id);
            log_file_write(log_content);
            ret = cloud_packet_rx_event_handler(msg);
            if (ret < 0) {
                log_file_write_fatal_error("invalid packet from cloud: %d\n",
                                           ret);
            }
        }
        if (msg->device_id == FROM_DSRC) {
            // printf("OBU_rx_event\n");
            if (Is_Heartbeat(msg) == 1) {
                Heartbeat_com_id = msg->handle_id;
            } else {
                // if(f_flag < 2)
                OBU_com_id = msg->handle_id;
                // f_flag++;
                ret = OBU_packet_rx_event_handler(msg);
                if (ret < 0) {
                    log_file_write_fatal_error("invalid packet from OBU: %d\n",
                                               ret);
                }
            }
        }
        if (msg->device_id == FROM_SMART_AVI) {
            AVI_com_id = msg->handle_id;
            Smart_AVI_packet_rx_event_handler(msg);
        }
        free(msg);
    }
    assert(threadpool_destroy(pool, 0) == 0);
    log_file_write_fatal_error("dispatcher thread exit");
}
