#ifndef QUEUE_H
#define QUEUE_H
#include <stddef.h>

// https://zhuanlan.zhihu.com/p/82738202
// 可以存放任何類型的void *
// 0: next ; 1:prev
typedef void *QUEUE[2];
/* Private macros. */
#define queue_next(q) (*(QUEUE **) &((*(q))[0]))
#define queue_prev(q) (*(QUEUE **) &((*(q))[1]))
#define queue_prev_next(q) (queue_next(queue_prev(q)))
#define queue_next_prev(q) (queue_prev(queue_next(q)))

/* Public macros. */
#define queue_data(ptr, type, field) \
    ((type *) ((char *) (ptr) -offsetof(type, field)))

/* Important note: mutating the list while queue_foreach is
 * iterating over its elements results undefined behavior.
 */
#define queue_foreach(q, h) \
    for ((q) = queue_next(h); (q) != (h); (q) = queue_next(q))

#define queue_empty(q) ((const QUEUE *) (q) == (const QUEUE *) queue_next(q))

#define queue_head(q) (queue_next(q))

#define queue_init(q)        \
    do {                     \
        queue_next(q) = (q); \
        queue_prev(q) = (q); \
    } while (0)

#define queue_add(h, n)                     \
    do {                                    \
        queue_prev_next(h) = queue_next(n); \
        queue_next_prev(n) = queue_prev(h); \
        queue_prev(h) = queue_prev(n);      \
        queue_prev_next(h) = (h);           \
    } while (0)

#define queue_insert_head(h, q)        \
    do {                               \
        queue_next(q) = queue_next(h); \
        queue_prev(q) = (h);           \
        queue_next_prev(q) = (q);      \
        queue_next(h) = (q);           \
    } while (0)

#define queue_insert_tail(h, q)        \
    do {                               \
        queue_next(q) = (h);           \
        queue_prev(q) = queue_prev(h); \
        queue_prev_next(q) = (q);      \
        queue_prev(h) = (q);           \
    } while (0)

#define queue_remove(q)                     \
    do {                                    \
        queue_prev_next(q) = queue_next(q); \
        queue_next_prev(q) = queue_prev(q); \
    } while (0)
#endif