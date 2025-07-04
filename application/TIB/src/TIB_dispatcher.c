#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>

#include "com_packet_processing.h"
#include "TIB_MAP_utils.h"
#include "TIB_SPaT_utils.h"
#include "TIB_TIM_utils.h"
#include "TIB_dispatcher.h"
#include "log.h"
#include "TIB_queue.h"
#include "TIB_config.h"
#include "server.h"
#include "threadpool.h"
#include "j2735_BroadcastList.h"

typedef struct J2735_msg_obj J2735_msg_obj_t;
J2735_msg_obj_t *latest_J2735_msg = NULL;   // 從J2735_broadcastlist 收到的最新封包
// pthread_mutex_t latest_J2735_msg_lock = PTHREAD_MUTEX_INITIALIZER; //互斥保護
volatile bool fetch_flag = false;

static atomic_int pending_msg_count = 0;  // 使用原子操作的計數器
static pthread_mutex_t latest_J2735_msg_lock;

LOG_USE_MODULE(TIB);

//最大公因數
int gcd(int m, int n){
    if (n == 0){
        return m;
    }
    else{
        return gcd(n, m % n);
    }
}
//最小公倍數
int lcm(int m, int n) {
    return m * n / gcd(m, n);
}

/**
 * 負責分派 TIB 封包送至對應 TIB_queue
 * SPAT、MAP、TIM 可以直接在 TIB 內讀取 config 做
 * EVA、RSA、PSM 等封包需要外部 APP 封裝後送至 middleware J2735_broadcastList 
 * TIB 透過註冊 callback 收到 J2735_broadcastList insert 通知並 fetch 資料
 * 最後將 J2735 封包 enqueue 進對應的 TIB_queue 中
 */
void TIB_dispatcher_handler()
{
    printf("Enter TIB dispatcher handler\n");
    
    map_queue_init();
    spat_queue_init();
    tim_queue_init();
    eva_queue_init();
    rsa_queue_init();
    psm_queue_init();

    //註冊外部 APP 透過middleware J2735 broadcastlist 送來的 callback function
    J2735_BroadcastList_register_callback(on_j2735_msg_ready); 
    
    int ret = 0;

    clock_t start_time, finish_time;
    int count = 0;
    double total_time = 0;
    int cycles = 0;
    int prior_stepID = -1;
    int prior_second = -1;
    int prior_planID = -1;
    int map_cycles = TIB_config.MAP_packet_cycle_per_transfer;
    int spat_cycles = TIB_config.SPaT_packet_cycle_per_transfer;
    int tim_cycles = TIB_config.TIM_packet_cycle_per_transfer;
    // 取各個傳送週期數的最小公倍數為一輪
    int max_cycles = map_cycles * spat_cycles / gcd(map_cycles, spat_cycles);
    for (;;) {
        printf("current cycle = %d\n", cycles);
    
        // 批量處理所有待處理的消息
        int msgs_to_process = atomic_load(&pending_msg_count);
        if (msgs_to_process > 0) {
            printf("Processing %d pending messages\n", msgs_to_process);
            
            for (int i = 0; i < msgs_to_process; i++) {
                pthread_mutex_lock(&latest_J2735_msg_lock);
                J2735_msg_obj_t *external_J2735_msg = J2735_BroadcastList_fetch();
                pthread_mutex_unlock(&latest_J2735_msg_lock);
                
                if (external_J2735_msg != NULL) {
                    // 原子性地減少計數器
                    atomic_fetch_sub(&pending_msg_count, 1);
                    
                    if(external_J2735_msg->data == NULL) {
                        printf("External data is null\n");
                        continue;
                    }
                    
                    // 處理消息（EVA, RSA, PSM 等）
                    process_external_message(external_J2735_msg, cycles);
                } 
                else {
                    // 如果沒有更多消息，重置計數器
                    atomic_store(&pending_msg_count, 0);
                    break;
                }
            }
        }

        if (cycles%map_cycles==0 && cycles!=0)
        {            
            int planID = get_plan_id();
        
            if (planID != prior_planID) {
                if (get_SubPhaseCount() < 0)
                    goto map_dispatch_end;
                if (map_msg_update(map) < 0)
                    goto map_dispatch_end;
                // printf("=========================\n");
                // map_print(map);
                prior_planID = planID;
            }
            struct tib_obj *_map_obj = tib_obj_create(map,MapData_Id);
            if (_map_obj != NULL) {
                printf("created MAP object\n");
                map_queue_enqueue(_map_obj);
                printf("map enqueue complete\n");
            }
            map_dispatch_end:{}
        }
        
        
        if (cycles%spat_cycles==0 && cycles!=0)
        {
            int stepID = get_current_step();
            int second = get_current_second();
            
            if (stepID != prior_stepID || prior_second != second) {
                if (spat_msg_update(p_spat) < 0)
                {
                    printf("SPAT update is not working\n");
                    printf("cycles new:%d\n",cycles); 
                    goto spat_dispatch_end;
                }
                prior_stepID = stepID;
                prior_second = second;
            }
            struct tib_obj *_spat_obj = tib_obj_create(p_spat,SPAT_Id);
            if (_spat_obj != NULL) {
                printf("created SPAT object\n");
                spat_queue_enqueue(_spat_obj);                
            }
            spat_dispatch_end:{}            
        }
        
        if (cycles%tim_cycles==0 && cycles!=0)
        {
            struct tib_obj *_tim_obj = tib_obj_create(p_tim,TravelerInformation_Id);
            if (_tim_obj != NULL) {
                printf("created TIM object\n");
                tim_queue_enqueue(_tim_obj);
            }
        }

        cycles++;
        if (cycles==(max_cycles*50)+1)
        {
            cycles=1;
            printf("cycles reset\n");
        }
        // sleep(0.5);
        
        //free(tib);
    }
    LOG_MSG_FATAL("dispatcher thread exit");
}

// 處理外部消息的輔助函數
void process_external_message(J2735_msg_obj_t *external_J2735_msg, int cycles) {
    // EVA 處理
    int eva_cycles = TIB_config.EVA_packet_cycle_per_transfer;
    int rsa_cycles = TIB_config.RSA_packet_cycle_per_transfer;
    int psm_cycles = TIB_config.PSM_packet_cycle_per_transfer;
    if (cycles % eva_cycles == 0 && cycles != 0) {
        if (external_J2735_msg->magId == EmergencyVehicleAlert_Id) {
            struct tib_obj *_eva_obj = tib_obj_create(external_J2735_msg->data, EmergencyVehicleAlert_Id);
            if (_eva_obj != NULL) {
                printf("created EVA object\n");
                eva_queue_enqueue(_eva_obj);
            } else {
                printf("eva obj is NULL\n");
            }
        }
    }
    
    // RSA 處理
    if (cycles % rsa_cycles == 0 && cycles != 0) {
        if (external_J2735_msg->magId == RoadSideAlert_Id) {
            struct tib_obj *_rsa_obj = tib_obj_create(external_J2735_msg->data, RoadSideAlert_Id);
            if (_rsa_obj != NULL) {
                printf("created RSA object\n");
                rsa_queue_enqueue(_rsa_obj);
            } else {
                printf("rsa obj is NULL\n");
            }
        }
    }
    
    // PSM 處理
    if (cycles % psm_cycles == 0 && cycles != 0) {
        if (external_J2735_msg->magId == PersonalSafetyMessage_Id) {
            struct tib_obj *_psm_obj = tib_obj_create(external_J2735_msg->data, PersonalSafetyMessage_Id);
            if (_psm_obj != NULL) {
                printf("created PSM object\n");
                psm_queue_enqueue(_psm_obj);
            } else {
                printf("psm obj is NULL\n");
            }
        }
    }
}

void on_j2735_msg_ready() {
    LOG_MSG_INFO("receive packet from J2735_broadcastList\n");

    atomic_fetch_add(&pending_msg_count, 1);
    // 可選：如果積累太多消息，可以記錄警告
    int current_count = atomic_load(&pending_msg_count);
    if (current_count > 10) {  // 閾值可調整
        printf("Warning: %d pending messages, producer may be too fast\n", current_count);
    }
}
