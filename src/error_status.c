#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>


#include "typedefine.h"
#include "error_status.h"
#include "timer_event.h"
#include "log.h"

uint8_t error_status=0;
uint8_t error_tc_5fcc=0;
timer_t dsrc_heartbeat_timer_id;
timer_t tc_5fcc_timer_id;

// static uint8_t err_count_5fcc=0;

pthread_mutex_t mutex_error_status = PTHREAD_MUTEX_INITIALIZER;

uint8_t get_error_status()
{
    pthread_mutex_lock(&mutex_error_status);
    uint8_t status = error_status;
    pthread_mutex_unlock(&mutex_error_status);
    return status;
}

void set_dsrc_error(__sigval_t value)
{
    pthread_mutex_lock(&mutex_error_status);
    // printf("don't get dsrc heartbeat packet and set dsrc err bit\r\n");
    error_status |= DSRC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_dsrc_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // log_file_write("get dsrc heartbeat packet and clear dsrc err bit\r\n");
    error_status &= ~DSRC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_tsc_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= TSC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_tsc_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~TSC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_disk_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // printf("disk error\n\r");
    error_status |= DISK_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_disk_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // printf("disk normal\n\r");
    error_status &= ~DISK_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_memory_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= MEMORY_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_memory_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~MEMORY_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}



//init error detect
void dsrc_error_detect_init(void){
    log_file_write("init dsrc error detect\r\n");
    create_timer(&dsrc_heartbeat_timer_id, NULL, set_dsrc_error);
    set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
}

//在tsp_paket_tx中 已經有定期回報的封包 裡面包含error status
//當發現有err bit被拉起來時 送錯誤封包到雲端

void tc_5fcc_error_detect_init(void){
    create_timer(&tc_5fcc_timer_id, NULL, set_tsc_5fcc_error);
    set_timer(tc_5fcc_timer_id, 0, 0, 10, 0);
}

void set_tsc_5fcc_error(__sigval_t value)
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= TCFAIL_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    log_file_write("get 5fcc error and going to restart program after 5sec\r\n");
    sleep(5);
    log_file_write("daemon get into kill self for not get 5fcc response\r\n");
    printf("daemon get into kill self for not get 5fcc response\r\n");
    kill(getpid(),SIGINT);

    return;
}

void clear_tsc_5fcc_error(void)
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~TCFAIL_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    //postpone the function of set_tsc_5fcc_error
    set_timer(tc_5fcc_timer_id, 0, 0, 10, 0);
    // err_count_5fcc=0;
    return;
}

