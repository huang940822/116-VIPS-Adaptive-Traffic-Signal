#ifndef BUF_H
#define BUF_H
#include <sys/types.h>

typedef struct {
	unsigned char *buff;
	size_t size;
} buffer_t;
#define DEFAULT_BUFF_SIZE 25600
buffer_t *alloc_buffer();
void free_buffer(buffer_t *buffer);
int get_buffer_size(buffer_t *buffer);
void decrease_buffer_size(buffer_t *buffer, size_t data_n);
void increase_buffer_size(buffer_t *buffer, size_t data_n);
#endif