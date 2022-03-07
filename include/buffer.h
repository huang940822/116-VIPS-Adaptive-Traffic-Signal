#ifndef BUF_H
#define BUF_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
	unsigned char *buff;
	size_t size;
} buffer_t;

typedef struct {
	buffer_t **record;
	u_int8_t max;
	u_int8_t first_record_pointer; // queue.front
    u_int8_t last_record_pointer; // queue.back
	pthread_mutex_t mutex;
} buffer_ring_t;

#define DEFAULT_BUFF_SIZE 25600
buffer_t *alloc_buffer();
buffer_ring_t *alloc_buffer_ring(int max);
void free_buffer(buffer_t *buffer);
void free_buffer_ring(buffer_ring_t *buffer_ring);
int get_buffer_size(buffer_t *buffer);
void decrease_buffer_size(buffer_t *buffer, size_t data_n);
void increase_buffer_size(buffer_t *buffer, size_t data_n);

bool buff_ring_empty(buffer_ring_t *object);
bool buff_ring_full(buffer_ring_t *object);
bool buff_ring_push(buffer_t *record, buffer_ring_t *object);
buffer_t *buff_ring_pop(buffer_ring_t *object);
#endif