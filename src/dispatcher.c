#include <pthread.h>
#include <stdio.h>

#include "com_packet_processing.h"
#include "dispatcher.h"
#include "log.h"
#include "msg_queue.h"
#include "server.h"
#include "threadpool.h"

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
    msg_queue_init();
    // pthread_mutex_init(&lock, NULL);
    int ret = 0;

    struct msg_obj *msg;
    // assert((pool = threadpool_create(THREAD, THREADQUEUE, 0)) != NULL);
    // fprintf(stderr,
    //         "Pool started with %d threads and "
    //         "queue size of %d\n",
    //         THREAD, THREADQUEUE);

    for (;;) {
        msg = msg_queue_dequeue();
        if (msg->device_id == FROM_CLOUD) {
            // printf("cloud_rx_event\n");

            cloud_com_id = msg->handle_id;
            log_file_write("cloud_com_id in dispatcher is %d", cloud_com_id);
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
