#include <sys/epoll.h>

#include "ae_event.h"
typedef struct ae_epoll_state {
    int epfd;
    struct epoll_event *events;
} ae_epoll_state;
int ae_epoll_create(ae_event_loop *event_loop)
{
    ae_epoll_state *state = malloc(sizeof(ae_epoll_state));;

    if (!state)
        return -1;
    state->events = malloc(sizeof(struct epoll_event) * event_loop->setsize);
    if (!state->events) {
        free(state);
        return -1;
    }
    state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (state->epfd == -1) {
        free(state->events);
        free(state);
        return -1;
    }
    event_loop->apidata = state;
    return 0;
}
void ae_epoll_free(ae_event_loop *event_loop)
{
    ae_epoll_state *state = event_loop->apidata;

    close(state->epfd);
    free(state->events);
    free(state);
}
int ae_epoll_add_event(ae_event_loop *event_loop, int fd, int mask)
{
    ae_epoll_state *state = event_loop->apidata;
    struct epoll_event ee = {0}; /* avoid valgrind warning */
    /* If the fd was already monitored for some event, we need a MOD
     * operation. Otherwise we need an ADD operation. */
    int op =
        event_loop->events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    mask |= event_loop->events[fd].mask; /* Merge old events */
    if (mask & AE_READABLE)
        ee.events |= (EPOLLIN);
    if (mask & AE_WRITABLE)
        ee.events |= (EPOLLOUT);
    ee.data.fd = fd;
    if (epoll_ctl(state->epfd, op, fd, &ee) == -1)
        return -1;
    return 0;
}
void ae_epoll_del_event(ae_event_loop *event_loop, int fd, int delmask)
{
    ae_epoll_state *state = event_loop->apidata;
    struct epoll_event ee = {0};
    int mask = event_loop->events[fd].mask & (~delmask);
    ee.events = 0;
    if (mask & AE_READABLE)
        ee.events |= (EPOLLIN);
    if (mask & AE_WRITABLE)
        ee.events |= (EPOLLOUT);
    ee.data.fd = fd;
    if (mask != AE_NONE) {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}
int ae_epoll_poll(ae_event_loop *event_loop, struct timeval *tvp)
{
    ae_epoll_state *state = event_loop->apidata;
    int retval, numevents = 0;
    retval = epoll_wait(state->epfd, state->events, event_loop->setsize,
                        tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec / 1000) : -1);
    if (retval > 0) {
        int j;
        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events + j;

            if (e->events & EPOLLIN)
                mask |= AE_READABLE;
            if (e->events & EPOLLOUT)
                mask |= AE_WRITABLE;
            if (e->events & EPOLLERR)
                mask |= AE_WRITABLE | AE_READABLE;
            if (e->events & EPOLLHUP)
                mask |= AE_WRITABLE | AE_READABLE;
            event_loop->fired[j].fd = e->data.fd;
            event_loop->fired[j].mask = mask;
        }
    }
    return numevents;
}
