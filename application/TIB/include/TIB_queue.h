#ifndef TIB_QUEUE_H
#define TIB_QUEUE_H

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
#define MAX_TIB_QUEUE_SIZE 50
#define MSG_Default_LEN 25600U


struct tib_obj {
    void *data;
    size_t msg_len;
    uint8_t tib_id; // 參照j2735_msg.h 中 DSRCmsgID詳述
    QUEUE queue;
};

struct tib_queue {
    QUEUE tib_queue_head;
    pthread_mutex_t mutex;
    sem_t full;
    sem_t empty;
};
//extern struct tib_queue tib_queue;
typedef struct tib_queue tib_queue_t;

struct tib_obj *map_queue_dequeue();

void map_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t map_queue_init();

struct tib_obj *spat_queue_dequeue();

void spat_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t spat_queue_init();

struct tib_obj *tim_queue_dequeue();

void tim_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t tim_queue_init();

struct tib_obj *eva_queue_dequeue();

void eva_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t eva_queue_init();

struct tib_obj *rsa_queue_dequeue();

void rsa_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t rsa_queue_init();

struct tib_obj *psm_queue_dequeue();

void psm_queue_enqueue(struct tib_obj *new_tib_obj);

int8_t psm_queue_init();

struct tib_obj *tib_obj_create(void *data,
                uint8_t tib_id);
#endif