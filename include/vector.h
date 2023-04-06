#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <string.h>

#define VECTOR_INIT_CAPACITY 4

#define vector_t(T)      \
    struct {             \
        size_t size;     \
        size_t capacity; \
        T *data;         \
    }

#define vector_init(v)                                         \
    do {                                                       \
        (v).size = 0;                                          \
        (v).capacity = VECTOR_INIT_CAPACITY;                   \
        (v).data = malloc(sizeof((v).data[0]) * (v).capacity); \
    } while (0)

#define vector_resize(v, capacity)                                         \
    do {                                                                   \
        void *_data = realloc((v).data, sizeof((v).data[0]) * (capacity)); \
        if (!_data)                                                        \
            abort();                                                       \
        (v).capacity = (capacity);                                         \
        (v).data = _data;                                                  \
    } while (0)

#define vector_push_back(v, value)                              \
    do {                                                        \
        if ((v).size == (v).capacity) {                         \
            int capacity = (v).capacity * 2;                    \
            vector_resize((v), capacity);                       \
        }                                                       \
        memcpy((v).data + (v).size++, &(value), sizeof(value)); \
    } while (0)

#define vector_pop_back(v)  \
    do {                    \
        if ((v).size > 0) { \
            (v).size--;     \
        }                   \
    } while (0)

#define vector_size(v) ((v).size)

#define vector_capacity(v) ((v).capacity)

#define vector_at(v, index) ((v).data[(index)])

#define vector_free(v)    \
    do {                  \
        free((v).data);   \
        (v).size = 0;     \
        (v).capacity = 0; \
        (v).data = NULL;  \
    } while (0)

#endif /* VECTOR_H */