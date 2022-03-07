#include "buffer.h"
#include "typedefine.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

buffer_t *alloc_buffer()
{
    buffer_t *buffer = malloc(sizeof(buffer_t));
    if (buffer == NULL) {
        goto err;
    }
    buffer->buff = malloc(DEFAULT_BUFF_SIZE);
    buffer->size = 0;
    return buffer;

err:
    if (buffer) {
        free(buffer->buff);
        free(buffer);
    }

    return NULL;
}

buffer_ring_t *alloc_buffer_ring(int max)
{
    buffer_ring_t *buffer_ring = malloc(sizeof(buffer_ring_t));
    memset(buffer_ring, 0, sizeof(buffer_ring_t));
    buffer_ring->max = max;
    buffer_ring->record = calloc(max, sizeof(buffer_t *));

    pthread_mutex_init(&buffer_ring->mutex, NULL);

    return buffer_ring;
}

void free_buffer(buffer_t *buffer)
{
    if (buffer) {
        free(buffer->buff);
        buffer->buff = NULL;
        free(buffer);
        buffer = NULL;
    }
}

void free_buffer_ring(buffer_ring_t *buffer_ring)
{
    if (buffer_ring) {
        if (buffer_ring->record) {
            while (!buff_ring_empty(buffer_ring)) {
                buffer_t *buff = buff_ring_pop(buffer_ring);
                free_buffer(buff);
            }
            free(buffer_ring->record);
        }
        free(buffer_ring);
    }
}

int get_buffer_size(buffer_t *buffer)
{
    return buffer->size;
}
void decrease_buffer_size(buffer_t *buffer, size_t data_n)
{
    if (buffer->size < data_n)
        buffer->size = 0;
    else
        buffer->size -= data_n;
}
void increase_buffer_size(buffer_t *buffer, size_t data_n)
{
    if (data_n > DEFAULT_BUFF_SIZE)
        buffer->size = DEFAULT_BUFF_SIZE;
    else
        buffer->size = data_n;
}

/*****************************************************************************
** Function:    buff_record_ring_empty
** Description: Check if buffer record ring is empty.
** Parameter:   front: front of ring
**              back : back of ring
** Return:      true : ring is empty
**              false: ring is not empty
******************************************************************************/
bool buff_ring_empty(buffer_ring_t *object)
{
    return (object->first_record_pointer == object->last_record_pointer);
}

/*****************************************************************************
** Function:    buff_ring_full
** Description: Check if buffer record ring is full.
** Parameter:   front: front of ring
**              back : back of ring
** Return:      true : ring is full
**              false: ring is not full
******************************************************************************/
bool buff_ring_full(buffer_ring_t *object)
{
    return ((object->last_record_pointer + 1) % object->max ==
            object->first_record_pointer);
}

/*****************************************************************************
** Function:    buff_ring_push
** Description: Push a record in buffer record ring.
                Use mutes make sure only one thread can push in one time.
** Parameter:   record: record to push
**              object: buffer obj with the same buffer id as the record
** Return:      none
******************************************************************************/
bool buff_ring_push(buffer_t *record, buffer_ring_t *object)
{
    pthread_mutex_lock(&object->mutex);
    if (buff_ring_full(object)) {
        pthread_mutex_unlock(&object->mutex);
        return false;
    }
    object->record[object->last_record_pointer] = record;
    object->last_record_pointer =
        (uint8_t)((object->last_record_pointer + 0x1) % object->max);
    pthread_mutex_unlock(&object->mutex);
    return true;
}

/*****************************************************************************
** Function:    buff_ring_push
** Description: Discard a record in buffer record ring.
                Use mutes make sure only one thread can push in one time.
** Parameter:   object: buffer obj to discard a record
** Return:      none
******************************************************************************/
buffer_t *buff_ring_pop(buffer_ring_t *object)
{
    pthread_mutex_lock(&object->mutex);
    if (buff_ring_empty(object)) {
        pthread_mutex_unlock(&object->mutex);
        return NULL;
    }
    buffer_t *tmp = object->record[object->first_record_pointer];
    object->first_record_pointer =
        (uint8_t)((object->first_record_pointer + 1) % object->max);
    pthread_mutex_unlock(&object->mutex);
    return tmp;
}
