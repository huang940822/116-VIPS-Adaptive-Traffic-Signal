#ifndef AE_EVENT_H
#define AE_EVENT_H
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>  // for close
#include <unistd.h>
#define AE_OK 0
#define AE_ERR -1
/*comm_event status*/
#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2

/*event flag*/
#define AE_COMM_EVENTS 1
/*timer event flag*/
#define AE_TIME_EVENTS 2
#define AE_ALL_EVENTS (AE_COMM_EVENTS | AE_TIME_EVENTS)
#define AE_DONT_WAIT 4
#define AE_DELETED_EVENT_ID -1
#define AE_ONESHOT -1
struct ae_event_loop;

/* Types and data structures */
typedef void ae_comm_process(struct ae_event_loop *eventLoop, int fd, void *clientData, int mask);
typedef int ae_time_process(struct ae_event_loop *eventLoop, long long id, void *clientData);
typedef void ae_event_time_destructor(struct ae_event_loop *eventLoop, void *clientData);
typedef void ae_before_sleep_prcess(struct ae_event_loop *eventLoop);

/* Comm event structure */
typedef struct ae_comm_event {
	int mask; /* one of AE_(NONE|READABLE|WRITABLE) */
	ae_comm_process *r_comm_proc;
	ae_comm_process *w_comm_proc;
	void *clientData;
} ae_comm_event;

/* A fired event */
typedef struct ae_fired_event {
	int fd;
	int mask; /* one of AE_(READABLE|WRITABLE) */
} ae_fired_event;

typedef struct ae_time_event {
	long long id; /*time event id*/
	/* In how mamy (seconds+1000*millseconds) later will this time event be triggered ?*/
	long when_sec; /*seconds*/
	long when_ms;  /* milliseconds */
	ae_time_process *time_proc;
	ae_event_time_destructor *destructor_proc;
	void *clientData;
	struct ae_time_event *next;
} ae_time_event;

/* State of an event based program */
typedef struct ae_event_loop {
	int maxfd;             /* highest file descriptor currently registered */
	int setsize;           /* max number of file descriptors tracked */
	ae_comm_event *events; /* Registered events */
	ae_fired_event *fired; /* Fired events */
	int stop;
	void *server;
	void *apidata; /* This is used for epoll API specific data */
	/*The following members are used in time event*/
	long long time_event_next_id;            /*Timed event ID*/
	time_t last_time;                        /*Time when the event was last processed, used to detect system clock skew */
	ae_time_event *time_event_head;          /*Timer event linked list header*/
	ae_before_sleep_prcess *before_sleep_fn; /*callback executed before sleep*/
} ae_event_loop;

/* Prototypes */
ae_event_loop *ae_create_event_loop(int setsize);

void ae_delete_event_loop(ae_event_loop *event_loop);

void ae_stop(ae_event_loop *event_loop);

int ae_create_comm_event(ae_event_loop *event_loop, int fd, int mask, ae_comm_process *proc, void *clientData);

void ae_delete_comm_event(ae_event_loop *event_loop, int fd, int mask);

int ae_get_comm_events(ae_event_loop *event_loop, int fd);

int ae_process_events(ae_event_loop *event_loop, int flags);

void ae_main(ae_event_loop *eventLoop);

long long ae_create_time_event(ae_event_loop *event_loop,
                               long long milliseconds,
                               ae_time_process *proc,
                               void *clientData,
                               ae_event_time_destructor *destructor);

int ae_delete_time_event(ae_event_loop *event_loop, long long id);

void ae_get_time(long *seconds, long *milliseconds);

void ae_add_milliseconds_to_now(long long milliseconds, long *sec, long *ms);

ae_time_event *ae_search_nearest_timer(ae_event_loop *event_loop);

int process_time_events(ae_event_loop *event_loop);

void ae_set_before_sleep_process(ae_event_loop *event_loop, ae_before_sleep_prcess *beforesleep);
#endif