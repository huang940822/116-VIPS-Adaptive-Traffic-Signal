#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "queue.h"
#include "util.h"

#define MSG_Q_OK 0
#define MSG_Q_ERR -1
#define MAX_MSG_QUEUE_SIZE 1024
#define MSG_Default_LEN 25600

#define CLOUD_BIT_POS (0)
#define CLOUD_BIT (BIT(CLOUD_BIT_POS))

#define DEVICE_DISCONNECT_BIT_POS (7)
#define DEVICE_DISCONNECT_BIT (BIT(DEVICE_DISCONNECT_BIT_POS))
#define IS_CLIENT_DISCONNECT(x) ((x) & (DEVICE_DISCONNECT_BIT))

#define DSRC_BIT_POS (1)
#define DSRC_BIT (BIT(DSRC_BIT_POS))

#define IS_FROM_CLOUD(x) ((x) & (CLOUD_BIT))
#define IS_FROM_DSRC(x) ((x) & (DSRC_BIT))

#define SET_DEVICE_ID(x, device_bit) (SET_BIT(x, device_bit))
#define SET_DEVICE_DISCONNECTED(x) (SET_BIT(x, DEVICE_DISCONNECT_BIT_POS))
#define CLEAR_DEVICE_ID(x, device_bit) (CLEAR_BIT(x, device_bit))

struct msg_obj {
    unsigned char msg[MSG_Default_LEN];
    size_t msg_len;
    uint8_t device_id;
    uint8_t handle_id;
    QUEUE queue;
};

struct msg_queue {
    QUEUE msg_queue_head;
    pthread_mutex_t mutex;
    sem_t full;
    sem_t empty;
};
extern struct msg_queue msg_queue;

struct msg_obj *msg_queue_dequeue();

void msg_queue_enqueue(struct msg_obj *new_msg_obj);

int8_t msg_queue_init();

struct msg_obj_t *msg_obj_create(buffer_t *buf,
                                 uint8_t device_id,
                                 uint8_t handle_id);
#endif