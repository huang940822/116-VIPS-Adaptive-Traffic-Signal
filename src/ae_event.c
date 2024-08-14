#include "ae_event.h"
#include "msg_queue.h"
/*
    Since epoll and its related system call can only be used in the Linux
   operating system, I especially use extern to separate declarations and
   definitions and avoid compile warnings
*/
extern int ae_epoll_create(ae_event_loop *event_loop);
extern void ae_epoll_free(ae_event_loop *event_loop);
extern int ae_epoll_add_event(ae_event_loop *event_loop, int fd, int mask);
extern void ae_epoll_del_event(ae_event_loop *event_loop, int fd, int delmask);
extern int ae_epoll_poll(ae_event_loop *event_loop, struct timeval *tvp);


ae_event_loop *ae_create_event_loop(int setsize)
{
    ae_event_loop *event_loop;
    int i;

    event_loop = malloc(sizeof(*event_loop));
    if (!event_loop)
        goto err;

    event_loop->events = malloc(sizeof(ae_comm_event) * setsize);
    event_loop->fired = malloc(sizeof(ae_fired_event) * setsize);

    if (!event_loop->events || !event_loop->fired)
        goto err;
    event_loop->setsize = setsize;
    event_loop->last_time = time(NULL);
    event_loop->time_event_head = NULL;
    event_loop->time_event_next_id = 0;
    event_loop->stop = 0;
    event_loop->maxfd = -1;
    event_loop->before_sleep_fn = NULL;
    if (ae_epoll_create(event_loop) == -1)
        goto err;
    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < setsize; i++)
        event_loop->events[i].mask = AE_NONE;
    return event_loop;

err:
    if (event_loop) {
        free(event_loop->events);
        free(event_loop->fired);
        free(event_loop);
    }
    return NULL;
}

void ae_delete_event_loop(ae_event_loop *event_loop)
{
    ae_epoll_free(event_loop);
    free(event_loop->events);
    free(event_loop->fired);
    free(event_loop);
}


void ae_stop(ae_event_loop *event_loop)
{
    event_loop->stop = 1;
}
/*
 * Monitor the status of the fd file based on the value of the mask parameter,
 * When fd is available, execute the proc function
 */
int ae_create_comm_event(ae_event_loop *event_loop,
                         int fd,
                         int mask,
                         ae_comm_process *proc,
                         void *clientData)
{
    if (fd >= event_loop->setsize)
        return AE_ERR;
    ae_comm_event *ce = &event_loop->events[fd];
    // monitor specified fd
    if (ae_epoll_add_event(event_loop, fd, mask) == -1)
        return AE_ERR;
    // setting comm_event type
    ce->mask |= mask;
    if (mask & AE_READABLE)
        ce->r_comm_proc = proc;
    if (mask & AE_WRITABLE)
        ce->w_comm_proc = proc;

    ce->clientData = clientData;
    // If necessary, update the maximum fd of the event handler
    if (fd > event_loop->maxfd)
        event_loop->maxfd = fd;
    return AE_OK;
}

void ae_delete_comm_event(ae_event_loop *event_loop, int fd, int mask)
{
    if (fd >= event_loop->setsize)
        return;
    ae_comm_event *ce = &event_loop->events[fd];

    // The type of the event to be monitored is not set, return directly
    if (ce->mask == AE_NONE)
        return;
    ae_epoll_del_event(event_loop, fd, mask);
    ce->mask = ce->mask & (~mask);
    if (fd == event_loop->maxfd && ce->mask == AE_NONE) {
        /* Update the max fd */
        int j;
        for (j = event_loop->maxfd - 1; j >= 0; j--)
            if (event_loop->events[j].mask != AE_NONE)
                break;
        event_loop->maxfd = j;
    }
}

int ae_get_comm_events(ae_event_loop *event_loop, int fd)
{
    if (fd >= event_loop->setsize)
        return 0;
    ae_comm_event *ce = &event_loop->events[fd];
    return ce->mask;
}
/* Process every pending comm event, then every triggered time event
 * (that may be registered by time event callbacks just processed).
 * Without special flags the function sleeps until some comm event
 * triggered, or when the next time event occurs (if any).
 *
 * If flags is 0, the function does nothing and returns.
 * if flags has AE_ALL_EVENTS set, all the kind of events are processed.
 * if flags has AE_COMM_EVENTS set, file events are processed.
 * if flags has AE_TIME_EVENTS set, time events are processed.
 * if flags has AE_DONT_WAIT set the function returns ASAP until all
 * the events that's possible to process without to wait are processed.
 *
 * The function returns the number of events processed. */
int ae_process_events(ae_event_loop *event_loop, int flags)
{
    int processed = 0, numevents;
    int j;
    /* Nothing to do? return ASAP */
    if (!(flags & AE_COMM_EVENTS) && !(flags & AE_TIME_EVENTS)) {
        printf("Nothing to do! return ASAP\n");
        return 0;
    }
    /*
     * looks for the time event that will be pending in the smallest amount of
     * time by calling ae_search_nearest_timer on the event loop
     */
    if (event_loop->maxfd != -1 ||
        ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        // process periodic time events
        ae_time_event *shortest = NULL;
        struct timeval tv, *tvp;

        /* if we didn't set DONT_WAIT flag */
        if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT)) {
            /* Find the timing event that needs to occur recently through the
             * for loop */
            shortest = ae_search_nearest_timer(event_loop);
        }
        if (shortest) {
            long now_sec, now_ms;
            /* get current time */
            ae_get_time(&now_sec, &now_ms);
            tvp = &tv;

            /* How many milliseconds we need to wait for the next time event to
             * be triggered? */
            /* Calculate the how many milliseconds we need to wait until the
             * latest time event occurs */
            long long ms = (shortest->when_sec - now_sec) * 1000 +
                           shortest->when_ms - now_ms;

            if (ms > 0) {
                /* If the timed event has not expired, calculate the waiting
                 * time,as the fourth parameter of epoll_wait */
                tvp->tv_sec = ms / 1000;
                tvp->tv_usec = (ms % 1000) * 1000;
            } else {
                /*otherwise setting time parameter to zero,which makes
                 * epoll_wait don't block*/
                tvp->tv_sec = 0;
                tvp->tv_usec = 0;
            }
        } else {
            /* At this time, the time event linked list is empty,
             * if we set the non-blocking flag(AE_DONT_WAIT), then set tv to 0
             * then there is no blocking, epoll_wait returns directly
             * Otherwise, just wait for the comm event to arrive*/
            if (flags & AE_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            } else {
                /* Otherwise we can block */
                tvp = NULL; /* wait forever */
            }
        }

        numevents = ae_epoll_poll(event_loop, tvp); //ae_epoll.c 
        printf("numevents = %d\n", numevents);
        //遍歷所有events並依序處理可寫和可讀的
        for (j = 0; j < numevents; j++) {
            ae_comm_event *ce = &event_loop->events[event_loop->fired[j].fd];
            int mask = event_loop->fired[j].mask;
            int fd = event_loop->fired[j].fd;
            int fired = 0;
            if (ce->mask & mask & AE_WRITABLE) {
                if (ce->w_comm_proc != ce->r_comm_proc) {
                    ce->w_comm_proc(event_loop, fd, ce->clientData, mask);
                    fired++;
                }
            }
            if (ce->mask & mask & AE_READABLE) {
                    fired++;
                    ce->r_comm_proc(event_loop, fd, ce->clientData, mask);
            }
            // printf("fired fd = %d\n", fd);
            processed++;
        }
    }

    /* Check time events,and process them*/
    if (flags & AE_TIME_EVENTS) {
        processed += process_time_events(event_loop);
    }
    return processed;
}


void ae_main(ae_event_loop *event_loop)
{
    event_loop->stop = 0;
    while (!event_loop->stop) {    
        // 根據設定的flag來處理通訊事件和定時事件，並返回處理的事件數量。通過 epoll 機制和自定義的定時事件管理，實現了高效的事件循環
        ae_process_events(event_loop, AE_ALL_EVENTS);
    }
    printf("\n/////////////////stopped////////////////////////////////\n");
}


long long ae_create_time_event(ae_event_loop *event_loop,
                               long long milliseconds,
                               ae_time_process *proc,
                               void *clientData,
                               ae_event_time_destructor *destructor)
{
    long long id = event_loop->time_event_next_id++;
    ae_time_event *te;
    te = malloc(sizeof(*te));

    if (te == NULL) {
        return AE_ERR;
    }

    te->id = id;
    /* Set the time to process the event*/
    ae_add_milliseconds_to_now(milliseconds, &te->when_sec, &te->when_ms);
    te->time_proc = proc;
    te->destructor_proc = destructor;
    te->clientData = clientData;
    /*appends new timer event to the list head*/
    te->next = event_loop->time_event_head;
    event_loop->time_event_head = te;

    return id;
}

/*
 * Delete the time event with a given id
 */
int ae_delete_time_event(ae_event_loop *event_loop, long long id)
{
    ae_time_event *te = event_loop->time_event_head;
    while (te) {
        if (te->id == id) {
            te->id = AE_DELETED_EVENT_ID;
            return AE_OK;
        }
        te = te->next;
    }
    return AE_ERR; /* NO event with the specified ID found */
}
/* Add milliseconds to the current time.*/
void ae_add_milliseconds_to_now(long long milliseconds, long *sec, long *ms)
{
    long cur_sec, cur_ms, when_sec, when_ms;
    /*Get current time*/
    ae_get_time(&cur_sec, &cur_ms);
    /* Calculate the number of seconds and milliseconds after adding
     * milliseconds*/
    when_sec = cur_sec + milliseconds / 1000;
    when_ms = cur_ms + milliseconds % 1000;
    /*
    If when_ms is greater than or equal to 1000
    then increase when_sec by one second
    */
    if (when_ms >= 1000) {
        when_sec++;
        when_ms -= 1000;
    }
    *sec = when_sec;
    *ms = when_ms;
}
/*
 * Retrieve the seconds and milliseconds of the current time,
 * and save them to the seconds and milliseconds parameters respectively
 */
void ae_get_time(long *seconds, long *milliseconds)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}
/*
 *	Search the first timer to be triggerd
 */
ae_time_event *ae_search_nearest_timer(ae_event_loop *event_loop)
{
    ae_time_event *te = event_loop->time_event_head;
    ae_time_event *nearest = NULL;

    while (te) {
        /*Find the first time event that is approaching to be expired and save
         * it to nearest*/
        if (!nearest || te->when_sec < nearest->when_sec ||
            (te->when_sec == nearest->when_sec &&
             te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}
/*
 *	Iterate over list of time events staring at event_loop->time_event_head.
 */
int process_time_events(ae_event_loop *event_loop)
{
    int processed = 0;
    ae_time_event *te, *prev;
    long long maxId;
    time_t now = time(NULL);

    /*
     * Here we try to find and deal with the time skew occurence,
     * if we find that the last time the time-event processed by us( a.k.a
     * event_loop->last_time ) was greater than the current time, this indicate
     * an occurence of time skew therefore we reset the time of the last
     * processing time event
     */
    if (now < event_loop->last_time) {
        te = event_loop->time_event_head;
        while (te) {
            te->when_sec = 0;
            te = te->next;
        }
    }
    /*Update the last_time to be the last processing time event*/
    event_loop->last_time = now;

    prev = NULL;
    te = event_loop->time_event_head;
    maxId = event_loop->time_event_next_id - 1;
    /*Iterate over time-event list*/
    while (te) {
        long now_sec, now_ms;
        long long id;
        //先處理預計要殺掉的time event在處理time event剩下time event要做的事情
        /* Remove events scheduled for deletion. */
        if (te->id == AE_DELETED_EVENT_ID) {
            ae_time_event *next = te->next;
            if (prev == NULL) {
                event_loop->time_event_head = te->next;
            } else {
                prev->next = te->next;
            }
            /*Explicity terminate the timer-event through the finalizer
             * (callback destructor) of that timer-event*/
            if (te->destructor_proc) {
                te->destructor_proc(event_loop, te->clientData);
            }
            free(te);
            te = next;
            continue;
        }

        /* Make sure we don't process time events created by time events in
         * this iteration. Note that this check is currently useless: we always
         * add new timers on the head, however if we change the implementation
         * detail, this check may be useful again: we keep it here for future
         * defense. */
        if (te->id > maxId) {
            te = te->next;
            continue;
        }
        /*get current time*/
        ae_get_time(&now_sec, &now_ms);
        /* if we find a time evnet which is already expired,then execute this
         * time event */
        if (now_sec > te->when_sec ||
            (now_sec == te->when_sec && now_ms >= te->when_ms)) {
            int retval;

            id = te->id;
            retval = te->time_proc(event_loop, id, te->clientData);
            processed++;
            /* if this happend to be periodic time event,
             *  then we prepare its next execution by setting its next expired
             * time */
            if (retval != AE_ONESHOT) {
                ae_add_milliseconds_to_now(retval, &te->when_sec, &te->when_ms);
            } else {
                /* perform lazy delete on one-shot time event */
                te->id = AE_DELETED_EVENT_ID;
            }
        }
        prev = te;
        te = te->next;
    }

    return processed;
}

//沒用到
void ae_set_before_sleep_process(ae_event_loop *event_loop,
                                 ae_before_sleep_prcess *beforesleep)
{
    event_loop->before_sleep_fn = beforesleep;
}