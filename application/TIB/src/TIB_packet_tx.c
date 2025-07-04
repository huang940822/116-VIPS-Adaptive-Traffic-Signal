#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "TIB.h"
#include "TIB_MAP_utils.h"
#include "TIB_SPaT_utils.h"
#include "TIB_TIM_utils.h"
#include "TIB_config.h"
#include "TIB_queue.h"
#include "TIB_packet_tx.h"
#include "byte_processing.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"

LOG_USE_MODULE(TIB);
uint8_t TIB_com_id = 0;


void *MAP_packet_tx_loop()
{
    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    int fd = set_timer_fd(TIB_config.MAP_packet_transfer_speed, "MAP_packet_tx_loop");
    int pior_planID = -1;
    clock_t start_time, finish_time;

    if (fd == -1) {
        return NULL;
    }

    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            LOG_MSG_FATAL("MAP_packet_tx_loop timer read error");

        int planID = get_plan_id();
        // 切換 plan 的時候才會算一次
        if (planID != pior_planID) {
            if (get_SubPhaseCount() < 0)
                continue;
            if (map_msg_update(map) < 0)
                continue;
            // LOG_MSG_TRACE("=========================");
            // map_print(map);
            pior_planID = planID;
        }
        // LOG_MSG_TRACE("=========================");
        // map_print(map);
        OBU_j2735_tx(MapData_Id, map);
        finish_time = clock();
        double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
        LOG_MSG_INFO("MAP_packet_tx cost time: %f\n",cost_time);
        printf("MAP_packet_tx cost time: %f\n",cost_time);
    }
    close(fd);
}

void *SPaT_packet_tx_loop()
{
    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    int pior_stepID = -1;
    int pior_second = -1;
    int fd = set_timer_fd(TIB_config.SPaT_packet_transfer_speed, "SPaT_packet_tx_loop");
    clock_t start_time, finish_time;

    if (fd == -1) {
        return NULL;
    }

    while (1) {
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            LOG_MSG_FATAL("SPaT_packet_tx_loop timer read error");
        int stepID = get_current_step();
        int second = get_current_second();
        // 在 stepID 換的時候更新
        printf("current step_ID: %d; prior step_ID: %d\n",stepID,pior_stepID);
        printf("current second: %d; prior second: %d\n",second,pior_second);
        if (stepID != pior_stepID || pior_second != second) {
            printf("attempt to update SPAT\n");
            if (spat_msg_update(p_spat) < 0)
            {
                printf("SPAT update is not working\n");
                //continue;
            }
                
            pior_stepID = stepID;
            pior_second = second;
            printf("moving on to OBU_Send\n");
        }
        // LOG_MSG_TRACE("=========================");
        // spat_printf(p_spat);
        OBU_j2735_tx(SPAT_Id, p_spat);
        finish_time = clock();
        double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
        LOG_MSG_INFO("SPAT_packet_tx cost time: %f\n",cost_time);
        printf("SPAT_packet_tx cost time: %f\n",cost_time);
    }
    close(fd);
}

/**
 * 用一條 thread 統一 TIB 封包推送
 * 在各式封包的 cycle dequeue 資料並推送
 * 
 */
void *general_packet_tx_loop() //各項封包傳播
{
    struct tib_obj *tib_map;
    struct tib_obj *tib_spat;
    struct tib_obj *tib_tim;
    struct tib_obj *tib_eva;
    struct tib_obj *tib_rsa;
    struct tib_obj *tib_psm;

    uint64_t exp;
    uint8_t *tx_buf = NULL;
    int tx_buf_len = 0;
    int cycles = 0;
    int map_cycles = TIB_config.MAP_packet_cycle_per_transfer;
    int spat_cycles = TIB_config.SPaT_packet_cycle_per_transfer;
    int tim_cycles = TIB_config.TIM_packet_cycle_per_transfer;
    int eva_cycles = TIB_config.EVA_packet_cycle_per_transfer;
    int rsa_cycles = TIB_config.RSA_packet_cycle_per_transfer;
    int psm_cycles = TIB_config.PSM_packet_cycle_per_transfer;
    // int tim_cycles = TIB_config.TIM_packet_cycle_per_transfer;
    int fd = set_timer_fd(1/(TIB_config.general_packet_time_per_cycle), "general_packet_tx_loop");
    clock_t start_time, finish_time;
    printf("start general packet tx loop\n");
    if (fd == -1) {
        return NULL;
    }

    while (1) {        
        
        int s = read(fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
            LOG_MSG_FATAL("general_packet_tx_loop timer read error");
        printf("general tx cycles: %d\n",cycles);
        
        if (cycles%map_cycles==0 && cycles!=0)
        {
            
            //tib_map = tib_queue_dequeue(map_queue);
            tib_map = map_queue_dequeue();
            if(tib_map != NULL){
                TIB_com_id = tib_map->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for MAP is %d", TIB_com_id);
                printf("sending MAP packet\n");
                start_time = clock();
                if (tib_map->data!=NULL){
                    printf("send obu j2735 map\n");
                    OBU_j2735_tx(MapData_Id,tib_map->data);
                }
                    
                free(tib_map);
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("MAP_packet_tx cost time: %f\n",cost_time);
            }
            else
                printf("no map msg to send\n");
        }
            
        
        
        if (cycles%spat_cycles==0 && cycles!=0)
        {
            start_time = clock();
            //tib_spat = tib_queue_dequeue(spat_queue);
            tib_spat = spat_queue_dequeue();
            if(tib_spat != NULL){
                TIB_com_id = tib_spat->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for SPAT is %d", TIB_com_id);
                printf("sending SPAT packet\n");
                if (tib_spat->data!=NULL){
                    printf("send obu j2735 spat\n");
                    OBU_j2735_tx(SPAT_Id, tib_spat->data);
                }
                //OBU_j2735_tx(SPAT_Id,tib_spat->data);            
                free(tib_spat);
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("SPAT_packet_tx cost time: %f\n",cost_time);
                }
            else
                printf("no spat msg to send\n");
        }

        if (cycles%tim_cycles==0 && cycles!=0)
        {
            start_time = clock();
            tib_tim = tim_queue_dequeue();
            if(tib_tim != NULL){
                TIB_com_id = tib_tim->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for TIM is %d", TIB_com_id);
                printf("sending TIM packet\n");
                if (tib_tim->data!=NULL){
                    printf("send obu j2735 tim\n");
                    OBU_j2735_tx(TravelerInformation_Id,tib_tim->data);
                }
                free(tib_tim);            
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("TIM_packet_tx cost time: %f\n",cost_time);
            }
            else
                printf("no tim msg to send\n");
            
        }
        
        if (cycles%eva_cycles==0 && cycles!=0)
        {
            start_time = clock();
            //tib_spat = tib_queue_dequeue(spat_queue);
            tib_eva = eva_queue_dequeue();
            if(tib_eva != NULL){
                TIB_com_id = tib_eva->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for EVA is %d", TIB_com_id);
                printf("sending EVA packet\n");
                if (tib_eva->data!=NULL){
                    printf("send obu j2735 eva\n");
                    OBU_j2735_tx(EmergencyVehicleAlert_Id, tib_eva->data);
                }
                //OBU_j2735_tx(SPAT_Id,tib_spat->data);            
                free(tib_eva);
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("EVA_packet_tx cost time: %f\n",cost_time);
            }
            else
                printf("no eva msg to send\n");
        }

        if (cycles%rsa_cycles==0 && cycles!=0)
        {
            tib_rsa = rsa_queue_dequeue();
            if(tib_rsa != NULL){
                TIB_com_id = tib_rsa->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for RSA is %d", TIB_com_id);
                printf("sending RSA packet\n");
                start_time = clock();
                if(tib_rsa->data != NULL){
                    printf("send obu j2735 rsa\n");
                    OBU_j2735_tx(RoadSideAlert_Id,tib_rsa->data);
                }
                
                free(tib_rsa);
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("RSA_packet_tx cost time: %f\n",cost_time);
            }
            else
                printf("no rsa msg to send\n");
        }

        if (cycles%psm_cycles==0 && cycles!=0)
        {
            tib_psm = psm_queue_dequeue();
            if(tib_psm != NULL){
                TIB_com_id = tib_psm->tib_id;
                LOG_MSG_INFO("TIB_com_id in TIB dispatcher for PSM is %d", TIB_com_id);
                printf("sending PSM packet\n");
                start_time = clock();
                if(tib_psm->data != NULL){
                    printf("send obu j2735 psm\n");
                    OBU_j2735_tx(PersonalSafetyMessage_Id, tib_psm->data);
                }
                
                free(tib_psm);
                finish_time = clock();
                double cost_time = ( double ) ( finish_time - start_time ) / CLOCKS_PER_SEC ;
                printf("PSM_packet_tx cost time: %f\n",cost_time);
            }
            else
                printf("no psm msg to send\n");
        }

        cycles++;
    }
    close(fd);
}

void TIB_send_ack()
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        LOG_MSG_FATAL("TIB_send_ack: malloc");
        perror("TIB_send_ack: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(0, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, TIB.id, write_buf.content);
    free(write_buf.content);
    return;
}
