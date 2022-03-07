#include "msg_queue.h"
#include "buffer.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
typedef struct msg_queue msg_queue_t;
typedef struct msg_obj msg_obj_t;
msg_queue_t msg_queue;
msg_obj_t *msg_queue_dequeue()
{
	// Wait until there's at least one item
	sem_wait(&(msg_queue.empty));
	pthread_mutex_lock(&(msg_queue.mutex));  // CRITICAL SECTION
	QUEUE *it = queue_head(&msg_queue.msg_queue_head);
	msg_obj_t *item = queue_data(it, msg_obj_t, queue);
	queue_remove(&item->queue);
	pthread_mutex_unlock(&(msg_queue.mutex));
	sem_post(&(msg_queue.full));
	return item;
}
void msg_queue_enqueue(msg_obj_t *new_msg_obj)
{
	// Wait until there's at least one space
	sem_wait(&(msg_queue.full));
	printf("msg queue is not full and then put msg into it\r\n");
	log_file_write("msg queue is not full and then put msg into it\r\n");
	pthread_mutex_lock(&msg_queue.mutex);  // CRITICAL SECTION
	queue_insert_tail(&msg_queue.msg_queue_head, &new_msg_obj->queue);
	pthread_mutex_unlock(&msg_queue.mutex);
	sem_post(&(msg_queue.empty));
}
int8_t msg_queue_init()
{
	queue_init(&msg_queue.msg_queue_head);
	if (pthread_mutex_init(&(msg_queue.mutex), NULL))
		return MSG_Q_ERR;
	if (sem_init(&(msg_queue.empty), 0, 0))
		return MSG_Q_ERR;
	if (sem_init(&(msg_queue.full), 0, MAX_MSG_QUEUE_SIZE))
		return MSG_Q_ERR;
	return MSG_Q_OK;
}
struct msg_obj_t *msg_obj_create(buffer_t *buf, uint8_t device_id, uint8_t handle_id)
{
	msg_obj_t *_msg_obj = malloc(sizeof(msg_obj_t));
	if (_msg_obj != NULL) {
		_msg_obj->device_id = device_id;
		memcpy(_msg_obj->msg, buf->buff, buf->size);
		_msg_obj->msg_len = buf->size;
		_msg_obj->handle_id = handle_id;
		return _msg_obj;
	}
	return NULL;
}