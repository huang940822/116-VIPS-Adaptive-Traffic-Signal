#include "SPM_repeater.h"
#include "SPM.h"
#include "log.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/timerfd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

pthread_t SPM_repeater_thread = 0;
pthread_mutex_t SPM_repeater_run_mutex = PTHREAD_MUTEX_INITIALIZER;

void SPM_repeater_start()
{
    pthread_mutex_lock(&SPM_repeater_run_mutex);
    if (SPM_repeater_thread == 0) {
        int ret = pthread_create(&SPM_repeater_thread, NULL, SPM_repeater, NULL);
        if (ret != 0) {
            log_file_write_fatal_error("error creating SPM_repeater_thread: %d", ret);
            perror("main: pthread_create");
            exit(errno);
        }
    }
    pthread_mutex_unlock(&SPM_repeater_run_mutex);
}

void *SPM_repeater()
{
    int fd = timerfd_create(CLOCK_REALTIME, 0), s;
    uint64_t exp;

    struct itimerspec timerValue;
    memset(&timerValue, 0, sizeof(struct itimerspec));

    timerValue.it_value.tv_sec = 0;
    timerValue.it_value.tv_nsec = 100000000;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = 100000000;

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timerValue, NULL) == -1) {
        log_file_write_fatal_error("SPM_repeater timerfd_settime");
    }

    while (SPM.dontSend2TC) {
        s = read(fd, &exp, sizeof(uint64_t));
        
    }
    SPM_repeater_thread = 0;
}
