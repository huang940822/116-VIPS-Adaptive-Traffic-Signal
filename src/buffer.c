#include "buffer.h"
#include "typedefine.h"

#include <assert.h>
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

void free_buffer(buffer_t *buffer)
{
	if (buffer) {
		free(buffer->buff);
		free(buffer);
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