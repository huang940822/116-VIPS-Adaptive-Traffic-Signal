#include "j2735_BroadcastList.h"
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "log.h"
#include "j2735_codec.h"

LOG_USE_MODULE(MIDDLEWARE);
static int is_initialized = 0; // 保證會先執行到 init 才可以被 APP insert

// 提供給 TIB 的 callback function 相關參數設定
typedef struct J2735_BroadcastList J2735_BroadcastList_t;
typedef struct J2735_msg_obj J2735_msg_obj_t;
J2735_BroadcastList_t J2735_BroadcastList;
typedef void (*BroadcastCallback)(void);
static BroadcastCallback broadcast_callback = NULL;
void J2735_BroadcastList_register_callback(BroadcastCallback cb) {
    broadcast_callback = cb;
}

/**
 * TIB 呼叫 J2735_broadcastlist_fetch 抓取資料，dequeue 一筆給TIB 
 */
J2735_msg_obj_t *J2735_BroadcastList_dequeue()
{
    if (sem_trywait(&(J2735_BroadcastList.empty)) != 0) {
        LOG_MSG_FATAL("J2735_BroadcastList dequeue empty\n");
        // queue 是空的，不阻塞，直接返回 NULL
        return NULL;
    }
    // sem_wait(&(J2735_BroadcastList.empty));
    pthread_mutex_lock(&(J2735_BroadcastList.mutex));  // CRITICAL SECTION
    QUEUE *it = queue_head(&J2735_BroadcastList.J2735_BroadcastList_head);
    J2735_msg_obj_t *item = queue_data(it, J2735_msg_obj_t, queue);
    queue_remove(&item->queue);
    pthread_mutex_unlock(&(J2735_BroadcastList.mutex));
    sem_post(&(J2735_BroadcastList.full));
    return item;
}

/**
 APP 透過呼叫 J2735_BroadcastList_insert，將J2735封包轉成J2735_obj
 再以 J2735_BroadcastList_enqueue 加入 J2735_BroadcastList
 */ 
void J2735_BroadcastList_enqueue(J2735_msg_obj_t *new_J2735_msg_obj)
{
    LOG_MSG_DEBUG("J2735_BroadcastList_enqueue\n");
    if (!is_initialized) {
        LOG_MSG_FATAL("ERROR: J2735_BroadcastList not initialized!\n");
        return;
    }
    // Wait until there's at least one space
    sem_wait(&(J2735_BroadcastList.full));
    // LOG_MSG_INFO("msg queue is not full and then put msg into it");
    pthread_mutex_lock(&J2735_BroadcastList.mutex);  // CRITICAL SECTION
    queue_insert_tail(&J2735_BroadcastList.J2735_BroadcastList_head, &new_J2735_msg_obj->queue);
    pthread_mutex_unlock(&J2735_BroadcastList.mutex);
    sem_post(&(J2735_BroadcastList.empty));
}

int8_t J2735_BroadcastList_init() {
    queue_init(&J2735_BroadcastList.J2735_BroadcastList_head);
    if (pthread_mutex_init(&(J2735_BroadcastList.mutex), NULL)) return MSG_Q_ERR;
    if (sem_init(&(J2735_BroadcastList.empty), 0, 0)) return MSG_Q_ERR;
    if (sem_init(&(J2735_BroadcastList.full), 0, MAX_J2735_BroadcastList_SIZE)) return MSG_Q_ERR;
    is_initialized = 1;
    return MSG_Q_OK;
}

/**
 * APP insert J2735 後創造一個節點
 * 將轉換後的 msgframe 餵進 _J2735_msg_obj
 */
J2735_msg_obj_t *J2735_msg_obj_create(MessageFrame msgf)
{
    LOG_MSG_DEBUG("J2735_msg_obj_create--------");
    J2735_msg_obj_t *_J2735_msg_obj = malloc(sizeof(J2735_msg_obj_t));
    if (_J2735_msg_obj != NULL) {
        _J2735_msg_obj->magId = msgf.messageId;
        _J2735_msg_obj->data = msgf.u.data;
        _J2735_msg_obj->msg_len = sizeof(msgf.u.data);
        LOG_MSG_DEBUG("successful\n");
        return _J2735_msg_obj;
    }
    LOG_MSG_FATAL("J2735_msg_obj_create unsuccessful\n");
    return NULL;
}

/**
 * 提供給 APP 傳入 J2735 封包的接口
 * 1. 先將封包 encode 成 msgf
 * 2. 插入J2735_broadcastlist
 * 3. 透過 callback 呼叫 TIB
 */
void J2735_BroadcastList_insert(DSRCmsgID magID, void* data)
{
    LOG_MSG_INFO("J2735_BroadcastList_insert\n");
    int buf_len;
    uint8_t *buf;
    J2735CodecErr err;
    char errmsg_buf[ERR_MSG_SZ];

    MessageFrame msgf;
    memset(&msgf, 0, sizeof(msgf));
    memset(&err, 0, sizeof(J2735CodecErr));

    err.msg_size = ERR_MSG_SZ;
    err.msg = errmsg_buf;

    msgf.messageId = magID;
    msgf.u.data = data;
    buf_len = j2735_msg_encode(&buf, &msgf, &err);

    struct J2735_msg_obj *_msg_obj =
        J2735_msg_obj_create(msgf);
    if (_msg_obj != NULL) {
        J2735_BroadcastList_enqueue(_msg_obj);
    }
    if (broadcast_callback != NULL) {
            broadcast_callback();
        }
}

// 提供 TIB 獲取其他 APP 封裝完暫存於 J2735_BroadcastList 的封包 API
J2735_msg_obj_t *J2735_BroadcastList_fetch()
{
    LOG_MSG_INFO("J2735_BroadcastList_fetch\n");
    struct J2735_msg_obj *j2735_TIB_msg;
    j2735_TIB_msg = J2735_BroadcastList_dequeue();
    return j2735_TIB_msg;
}