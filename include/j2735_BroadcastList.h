#ifndef J2735_BroadcastList_H
#define J2735_BroadcastList_H
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
#define MAX_J2735_BroadcastList_SIZE 1024
#define MSG_Default_LEN 25600U

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


struct J2735_msg_obj {
    unsigned char msg[MSG_Default_LEN];
    size_t msg_len;
    DSRCmsgID magId;
    void* data;
    QUEUE queue;
};

struct J2735_BroadcastList {
    QUEUE J2735_BroadcastList_head;
    pthread_mutex_t mutex;
    sem_t full;
    sem_t empty;
};
extern struct J2735_BroadcastList J2735_BroadcastList;

void J2735_BroadcastList_insert(DSRCmsgID magID, void *data);

struct J2735_msg_obj *J2735_BroadcastList_fetch();

struct J2735_msg_obj *J2735_BroadcastList_dequeue();

void J2735_BroadcastList_enqueue(struct J2735_msg_obj *new_J2735_msg_obj);

int8_t J2735_BroadcastList_init();

struct J2735_msg_obj *J2735_msg_obj_create(MessageFrame msgf);

#endif