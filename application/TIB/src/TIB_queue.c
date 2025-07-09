#include "TIB_queue.h"
#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "log.h"
LOG_USE_MODULE(TIB);

typedef struct tib_obj tib_obj_t;
tib_queue_t spat_queue;
tib_queue_t map_queue;
tib_queue_t tim_queue;
tib_queue_t eva_queue;
tib_queue_t rsa_queue;
tib_queue_t psm_queue;

// 添加隊列狀態檢查函數
int map_queue_size() {
    int current_items;
    sem_getvalue(&(map_queue.empty), &current_items);
    return current_items;
}

int map_queue_capacity() {
    int available_slots;
    sem_getvalue(&(map_queue.full), &available_slots);
    return available_slots + map_queue_size();
}

// 打印隊列狀態的輔助函數
void print_map_queue_status(const char* operation) {
    int current_size = map_queue_size();
    int capacity = map_queue_capacity();
    
    printf("[%s] MAP Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *map_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_map_queue_status("MAP_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(map_queue.empty)) != 0) {
        printf("Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue items...\n");
    
    // 4. 進入臨界區
    pthread_mutex_lock(&(map_queue.mutex));
    
    // 5. 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&map_queue.tib_queue_head);
    if (queue_node == NULL) {
        // 防御性編程：雖然理論上不應該發生
        pthread_mutex_unlock(&(map_queue.mutex));
        printf("fatal error: head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 6. 離開臨界區
    pthread_mutex_unlock(&(map_queue.mutex));
    
    // 7. 釋放full信號量（表示有空間可用）
    sem_post(&(map_queue.full));
    
    // 8. 打印最終狀態
    print_map_queue_status("MAP_DEQUEUE_END");
    printf("dequeue successful\n\n");
    
    return item;
}

void map_queue_enqueue(tib_obj_t *new_tib_obj) //選擇指定queue並插入TIB推播訊息
{
    // Wait until there's at least one space    
    sem_wait(&(map_queue.full));
    printf("acquired empty sem\n");
    pthread_mutex_lock(&map_queue.mutex);  // CRITICAL SECTION
    printf("lock map_queue\n");
    queue_insert_tail(&map_queue.tib_queue_head, &new_tib_obj->queue);
    printf("map queue next empty: %ld\n",map_queue.empty.__align);
    pthread_mutex_unlock(&map_queue.mutex);
    sem_post(&(map_queue.empty));    
}

int8_t map_queue_init()
{
    queue_init(&map_queue.tib_queue_head);
    if (pthread_mutex_init(&(map_queue.mutex), NULL))
    {
        printf("map queue init suffered NULL\n");
        return MSG_Q_ERR;
    }        
    else
    {
        printf("map queue mutex init\n");
    }
    if (sem_init(&(map_queue.empty), 0, 0))
    {
        printf("map queue init not empty \n");
        return MSG_Q_ERR;
    }
    else
    {
        printf("map queue empty state init\n");
    } 
    if (sem_init(&(map_queue.full), 0, MAX_TIB_QUEUE_SIZE))
    {
        printf("map queue init not full \n");
        return MSG_Q_ERR;
    }
    else
    {
        printf("map queue init is full \n");
    }
    printf("map queue init success\n");
    LOG_MSG_INFO("map queue init success\n");
    return MSG_Q_OK;
}

// ==================== SPAT QUEUE ====================

// 添加隊列狀態檢查函數
int spat_queue_size() {
    int current_items;
    sem_getvalue(&(spat_queue.empty), &current_items);
    return current_items;
}

int spat_queue_capacity() {
    int available_slots;
    sem_getvalue(&(spat_queue.full), &available_slots);
    return available_slots + spat_queue_size();
}

// 打印隊列狀態的輔助函數
void print_spat_queue_status(const char* operation) {
    int current_size = spat_queue_size();
    int capacity = spat_queue_capacity();
    
    printf("[%s] SPAT Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *spat_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_spat_queue_status("SPAT_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(spat_queue.empty)) != 0) {
        printf("SPAT Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue spat items...\n");
    
    // 進入臨界區
    pthread_mutex_lock(&(spat_queue.mutex));
    
    // 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&spat_queue.tib_queue_head);
    if (queue_node == NULL) {
        pthread_mutex_unlock(&(spat_queue.mutex));
        printf("fatal error: spat head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 離開臨界區
    pthread_mutex_unlock(&(spat_queue.mutex));
    
    // 釋放full信號量（表示有空間可用）
    sem_post(&(spat_queue.full));
    
    // 打印最終狀態
    print_spat_queue_status("SPAT_DEQUEUE_END");
    printf("spat dequeue successful\n\n");
    
    return item;
}

void spat_queue_enqueue(tib_obj_t *new_spat_obj) {
    // Wait until there's at least one space    
    sem_wait(&(spat_queue.full));
    printf("acquired spat empty sem\n");
    pthread_mutex_lock(&spat_queue.mutex);  // CRITICAL SECTION
    printf("lock spat_queue\n");
    queue_insert_tail(&spat_queue.tib_queue_head, &new_spat_obj->queue);
    printf("spat queue next empty: %ld\n",spat_queue.empty.__align);
    pthread_mutex_unlock(&spat_queue.mutex);
    sem_post(&(spat_queue.empty));    
}

int8_t spat_queue_init() {
    queue_init(&spat_queue.tib_queue_head);
    if (pthread_mutex_init(&(spat_queue.mutex), NULL)) {
        printf("spat queue init suffered NULL\n");
        return MSG_Q_ERR;
    } else {
        printf("spat queue mutex init\n");
    }
    if (sem_init(&(spat_queue.empty), 0, 0)) {
        printf("spat queue init not empty \n");
        return MSG_Q_ERR;
    } else {
        printf("spat queue empty state init\n");
    } 
    if (sem_init(&(spat_queue.full), 0, MAX_TIB_QUEUE_SIZE)) {
        printf("spat queue init not full \n");
        return MSG_Q_ERR;
    } else {
        printf("spat queue init is full \n");
    }
    printf("spat queue init success\n");
    LOG_MSG_INFO("spat queue init success\n");
    return MSG_Q_OK;
}

// ==================== TIM QUEUE ====================

// 添加隊列狀態檢查函數
int tim_queue_size() {
    int current_items;
    sem_getvalue(&(tim_queue.empty), &current_items);
    return current_items;
}

int tim_queue_capacity() {
    int available_slots;
    sem_getvalue(&(tim_queue.full), &available_slots);
    return available_slots + tim_queue_size();
}

// 打印隊列狀態的輔助函數
void print_tim_queue_status(const char* operation) {
    int current_size = tim_queue_size();
    int capacity = tim_queue_capacity();
    
    printf("[%s] TIM Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *tim_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_tim_queue_status("TIM_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(tim_queue.empty)) != 0) {
        printf("TIM Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue tim items...\n");
    
    // 進入臨界區
    pthread_mutex_lock(&(tim_queue.mutex));
    
    // 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&tim_queue.tib_queue_head);
    if (queue_node == NULL) {
        pthread_mutex_unlock(&(tim_queue.mutex));
        printf("fatal error: tim head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 離開臨界區
    pthread_mutex_unlock(&(tim_queue.mutex));
    
    // 釋放full信號量（表示有空間可用）
    sem_post(&(tim_queue.full));
    
    // 打印最終狀態
    print_tim_queue_status("TIM_DEQUEUE_END");
    printf("tim dequeue successful\n\n");
    
    return item;
}

void tim_queue_enqueue(tib_obj_t *new_tim_obj) {
    // Wait until there's at least one space    
    sem_wait(&(tim_queue.full));
    printf("acquired tim empty sem\n");
    pthread_mutex_lock(&tim_queue.mutex);  // CRITICAL SECTION
    printf("lock tim_queue\n");
    queue_insert_tail(&tim_queue.tib_queue_head, &new_tim_obj->queue);
    printf("tim queue next empty: %ld\n",tim_queue.empty.__align);
    pthread_mutex_unlock(&tim_queue.mutex);
    sem_post(&(tim_queue.empty));    
}

int8_t tim_queue_init() {
    queue_init(&tim_queue.tib_queue_head);
    if (pthread_mutex_init(&(tim_queue.mutex), NULL)) {
        printf("tim queue init suffered NULL\n");
        return MSG_Q_ERR;
    } else {
        printf("tim queue mutex init\n");
    }
    if (sem_init(&(tim_queue.empty), 0, 0)) {
        printf("tim queue init not empty \n");
        return MSG_Q_ERR;
    } else {
        printf("tim queue empty state init\n");
    } 
    if (sem_init(&(tim_queue.full), 0, MAX_TIB_QUEUE_SIZE)) {
        printf("tim queue init not full \n");
        return MSG_Q_ERR;
    } else {
        printf("tim queue init is full \n");
    }
    printf("tim queue init success\n");
    LOG_MSG_INFO("tim queue init success\n");
    return MSG_Q_OK;
}

// ==================== EVA QUEUE ====================

// 添加隊列狀態檢查函數
int eva_queue_size() {
    int current_items;
    sem_getvalue(&(eva_queue.empty), &current_items);
    return current_items;
}

int eva_queue_capacity() {
    int available_slots;
    sem_getvalue(&(eva_queue.full), &available_slots);
    return available_slots + eva_queue_size();
}

// 打印隊列狀態的輔助函數
void print_eva_queue_status(const char* operation) {
    int current_size = eva_queue_size();
    int capacity = eva_queue_capacity();
    
    printf("[%s] EVA Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *eva_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_eva_queue_status("EVA_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(eva_queue.empty)) != 0) {
        printf("EVA Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue eva items...\n");
    
    // 進入臨界區
    pthread_mutex_lock(&(eva_queue.mutex));
    
    // 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&eva_queue.tib_queue_head);
    if (queue_node == NULL) {
        pthread_mutex_unlock(&(eva_queue.mutex));
        printf("fatal error: eva head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 離開臨界區
    pthread_mutex_unlock(&(eva_queue.mutex));
    
    // 釋放full信號量（表示有空間可用）
    sem_post(&(eva_queue.full));
    
    // 打印最終狀態
    print_eva_queue_status("EVA_DEQUEUE_END");
    printf("eva dequeue successful\n\n");
    
    return item;
}

void eva_queue_enqueue(tib_obj_t *new_eva_obj) {
    // Wait until there's at least one space    
    sem_wait(&(eva_queue.full));
    printf("acquired eva empty sem\n");
    pthread_mutex_lock(&eva_queue.mutex);  // CRITICAL SECTION
    printf("lock eva_queue\n");
    queue_insert_tail(&eva_queue.tib_queue_head, &new_eva_obj->queue);
    printf("eva queue next empty: %ld\n",eva_queue.empty.__align);
    pthread_mutex_unlock(&eva_queue.mutex);
    sem_post(&(eva_queue.empty));    
}

int8_t eva_queue_init() {
    queue_init(&eva_queue.tib_queue_head);
    if (pthread_mutex_init(&(eva_queue.mutex), NULL)) {
        printf("eva queue init suffered NULL\n");
        return MSG_Q_ERR;
    } else {
        printf("eva queue mutex init\n");
    }
    if (sem_init(&(eva_queue.empty), 0, 0)) {
        printf("eva queue init not empty \n");
        return MSG_Q_ERR;
    } else {
        printf("eva queue empty state init\n");
    } 
    if (sem_init(&(eva_queue.full), 0, MAX_TIB_QUEUE_SIZE)) {
        printf("eva queue init not full \n");
        return MSG_Q_ERR;
    } else {
        printf("eva queue init is full \n");
    }
    printf("eva queue init success\n");
    LOG_MSG_INFO("eva queue init success\n");
    return MSG_Q_OK;
}

// ==================== RSA QUEUE ====================

// 添加隊列狀態檢查函數
int rsa_queue_size() {
    int current_items;
    sem_getvalue(&(rsa_queue.empty), &current_items);
    return current_items;
}

int rsa_queue_capacity() {
    int available_slots;
    sem_getvalue(&(rsa_queue.full), &available_slots);
    return available_slots + rsa_queue_size();
}

// 打印隊列狀態的輔助函數
void print_rsa_queue_status(const char* operation) {
    int current_size = rsa_queue_size();
    int capacity = rsa_queue_capacity();
    
    printf("[%s] RSA Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *rsa_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_rsa_queue_status("RSA_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(rsa_queue.empty)) != 0) {
        printf("RSA Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue rsa items...\n");
    
    // 進入臨界區
    pthread_mutex_lock(&(rsa_queue.mutex));
    
    // 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&rsa_queue.tib_queue_head);
    if (queue_node == NULL) {
        pthread_mutex_unlock(&(rsa_queue.mutex));
        printf("fatal error: rsa head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 離開臨界區
    pthread_mutex_unlock(&(rsa_queue.mutex));
    
    // 釋放full信號量（表示有空間可用）
    sem_post(&(rsa_queue.full));
    
    // 打印最終狀態
    print_rsa_queue_status("RSA_DEQUEUE_END");
    printf("rsa dequeue successful\n\n");
    
    return item;
}

void rsa_queue_enqueue(tib_obj_t *new_rsa_obj) {
    // Wait until there's at least one space    
    sem_wait(&(rsa_queue.full));
    printf("acquired rsa empty sem\n");
    pthread_mutex_lock(&rsa_queue.mutex);  // CRITICAL SECTION
    printf("lock rsa_queue\n");
    queue_insert_tail(&rsa_queue.tib_queue_head, &new_rsa_obj->queue);
    printf("rsa queue next empty: %ld\n",rsa_queue.empty.__align);
    pthread_mutex_unlock(&rsa_queue.mutex);
    sem_post(&(rsa_queue.empty));    
}

int8_t rsa_queue_init() {
    queue_init(&rsa_queue.tib_queue_head);
    if (pthread_mutex_init(&(rsa_queue.mutex), NULL)) {
        printf("rsa queue init suffered NULL\n");
        return MSG_Q_ERR;
    } else {
        printf("rsa queue mutex init\n");
    }
    if (sem_init(&(rsa_queue.empty), 0, 0)) {
        printf("rsa queue init not empty \n");
        return MSG_Q_ERR;
    } else {
        printf("rsa queue empty state init\n");
    } 
    if (sem_init(&(rsa_queue.full), 0, MAX_TIB_QUEUE_SIZE)) {
        printf("rsa queue init not full \n");
        return MSG_Q_ERR;
    } else {
        printf("rsa queue init is full \n");
    }
    printf("rsa queue init success\n");
    LOG_MSG_INFO("rsa queue init success\n");
    return MSG_Q_OK;
}

// ==================== PSM QUEUE ====================

// 添加隊列狀態檢查函數
int psm_queue_size() {
    int current_items;
    sem_getvalue(&(psm_queue.empty), &current_items);
    return current_items;
}

int psm_queue_capacity() {
    int available_slots;
    sem_getvalue(&(psm_queue.full), &available_slots);
    return available_slots + psm_queue_size();
}

// 打印隊列狀態的輔助函數
void print_psm_queue_status(const char* operation) {
    int current_size = psm_queue_size();
    int capacity = psm_queue_capacity();
    
    printf("[%s] PSM Queue status: %d/%d (used / capacity)\n", 
           operation, current_size, capacity);
}

// 優化後的dequeue函數
tib_obj_t *psm_queue_dequeue() {
    // 先檢查並打印當前隊列狀態
    print_psm_queue_status("PSM_DEQUEUE_START");
    
    // 嘗試獲取empty信號量（表示有項目可取）
    if (sem_trywait(&(psm_queue.empty)) != 0) {
        printf("PSM Queue is empty, cannot dequeue\n");
        return NULL;
    }
    
    printf("start dequeue psm items...\n");
    
    // 進入臨界區
    pthread_mutex_lock(&(psm_queue.mutex));
    
    // 從隊列頭取出項目
    QUEUE *queue_node = queue_head(&psm_queue.tib_queue_head);
    if (queue_node == NULL) {
        pthread_mutex_unlock(&(psm_queue.mutex));
        printf("fatal error: psm head is null\n");
        return NULL;
    }
    
    tib_obj_t *item = queue_data(queue_node, tib_obj_t, queue);
    queue_remove(&item->queue);
    
    // 離開臨界區
    pthread_mutex_unlock(&(psm_queue.mutex));
    
    // 釋放full信號量（表示有空間可用）
    sem_post(&(psm_queue.full));
    
    // 打印最終狀態
    print_psm_queue_status("PSM_DEQUEUE_END");
    printf("psm dequeue successful\n\n");
    
    return item;
}

void psm_queue_enqueue(tib_obj_t *new_psm_obj) {
    // Wait until there's at least one space    
    sem_wait(&(psm_queue.full));
    printf("acquired psm empty sem\n");
    pthread_mutex_lock(&psm_queue.mutex);  // CRITICAL SECTION
    printf("lock psm_queue\n");
    queue_insert_tail(&psm_queue.tib_queue_head, &new_psm_obj->queue);
    printf("psm queue next empty: %ld\n",psm_queue.empty.__align);
    pthread_mutex_unlock(&psm_queue.mutex);
    sem_post(&(psm_queue.empty));    
}

int8_t psm_queue_init() {
    queue_init(&psm_queue.tib_queue_head);
    if (pthread_mutex_init(&(psm_queue.mutex), NULL)) {
        printf("psm queue init suffered NULL\n");
        return MSG_Q_ERR;
    } else {
        printf("psm queue mutex init\n");
    }
    if (sem_init(&(psm_queue.empty), 0, 0)) {
        printf("psm queue init not empty \n");
        return MSG_Q_ERR;
    } else {
        printf("psm queue empty state init\n");
    } 
    if (sem_init(&(psm_queue.full), 0, MAX_TIB_QUEUE_SIZE)) {
        printf("psm queue init not full \n");
        return MSG_Q_ERR;
    } else {
        printf("psm queue init is full \n");
    }
    printf("psm queue init success\n");
    LOG_MSG_INFO("psm queue init success\n");
    return MSG_Q_OK;
}

tib_obj_t *tib_obj_create(void *data,
    uint8_t tib_id)
{
    tib_obj_t *_tib_obj = malloc(sizeof(tib_obj_t));
    printf("created TIB object\n");
    if (_tib_obj != NULL) {
        _tib_obj->data = data;
        _tib_obj->msg_len = sizeof(data);
        _tib_obj->tib_id = tib_id;
        printf("return tib obj\n");
        return _tib_obj;
    }
    return NULL;
}
