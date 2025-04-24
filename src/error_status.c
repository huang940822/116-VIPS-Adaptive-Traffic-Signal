#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>


#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "typedefine.h"

LOG_USE_MODULE(MIDDLEWARE);

/* error_status 說明:
Bit 0: DSRC
Bit 1: VMS異常
Bit 2: DISK
Bit 3: MEMORY
Bit 4: 號控箱死當
Bit 5: 655xx_error
Bit 6: 雲端下的更新補償策略封包策略二phase weight錯誤
Bit 7: 時間異常
*/

uint8_t error_status = 0;

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
    // LOG_MSG_TRACE("don't get dsrc heartbeat packet and set dsrc err bit");
    error_status |= DSRC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_dsrc_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // LOG_MSG_INFO("get dsrc heartbeat packet and clear dsrc err bit");
    error_status &= ~DSRC_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_vms_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= VMS_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_vms_error()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~VMS_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_disk_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // LOG_MSG_TRACE("disk error\n");
    error_status |= DISK_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_disk_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // LOG_MSG_TRACE("disk normal\n");
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



// init error detect
void dsrc_error_detect_init(void)
{
    LOG_MSG_INFO("init dsrc error detect");
    create_timer(&dsrc_heartbeat_timer_id, NULL, set_dsrc_error);
    set_timer(dsrc_heartbeat_timer_id, 0, 0, 10, 0);
}

//在tsp_paket_tx中 已經有定期回報的封包 裡面包含error status
//當發現有err bit被拉起來時 送錯誤封包到雲端

void tc_5fcc_error_detect_init(void)
{
    create_timer(&tc_5fcc_timer_id, NULL, set_tsc_5fcc_error);
    set_timer(tc_5fcc_timer_id, 0, 0, 60, 0);
}

void set_tsc_5fcc_error(__sigval_t value)
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= TCFAIL_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    LOG_MSG_INFO(
        "get 5fcc error and going to restart program after 5sec");
    sleep(5);
    LOG_MSG_INFO("daemon get into kill self for not get 5fcc response");
    kill(getpid(), SIGINT);

    return;
}

void clear_tsc_5fcc_error(void)
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~TCFAIL_BIT_POSITION;
    pthread_mutex_unlock(&mutex_error_status);
    // postpone the function of set_tsc_5fcc_error
    set_timer(tc_5fcc_timer_id, 0, 0, 60, 0);
    // err_count_5fcc=0;
    return;
}


void set_655xx_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // LOG_MSG_TRACE("don't get dsrc heartbeat packet and set dsrc err bit");
    error_status |= TC_655XX_ERR;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_655xx_error()
{
    pthread_mutex_lock(&mutex_error_status);
    // LOG_MSG_INFO("get dsrc heartbeat packet and clear dsrc err bit");
    error_status &= ~TC_655XX_ERR;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void set_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status |= CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}

void clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR()
{
    pthread_mutex_lock(&mutex_error_status);
    error_status &= ~CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR;
    pthread_mutex_unlock(&mutex_error_status);
    return;
}
