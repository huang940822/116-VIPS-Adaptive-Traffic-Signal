#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "EVSP.h"
#include "EVSP_OBU_list.h"
#include "EVSP_config.h"
#include "EVSP_packet_tx.h"
#include "EVSP_subphase_threshold.h"
#include "EVSP_timer_event.h"
#include "EVSP_touching_area.h"
#include "EVSP_typedefine.h"
#include "application_registration.h"
#include "byte_processing.h"
#include "cms.h"
#include "com_packet_processing.h"
#include "config.h"
#include "error_status.h"
#include "gps_information.h"
#include "j2735_msg.h"
#include "j2735_srm.h"
#include "log.h"
#include "timer_event.h"
#include "traffic_compensation.h"
#include "traffic_signal_command_buffer.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"
#include "vms.h"

LOG_USE_MODULE(EVSP);

extern uint8_t activate_amount; //已觸發的EVSP數量
pthread_mutex_t mutex_active_EVSP = PTHREAD_MUTEX_INITIALIZER;
app_obj_t EVSP = {
    .name = "EVSP",
    .id = EVSP_ID,
    .priority = 1,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &EVSP_on_CLOUD_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &EVSP_on_registration,
    .next = NULL,
    .dontSend2TC = 0,
};


int EVSP_on_CLOUD_packet_rx(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    //開始讀取雲端封包
    msg_buf_t read_buf;
    read_buf.index = 0;
    Malloc(read_buf.content, app_section->payload_len, "EVSP_on_cloud_packet_rx");
    if (read_buf.content == NULL)
        return -1;
    memcpy(read_buf.content, app_section->payload, app_section->payload_len);

    // needs a evsp send ack function to send ack to cloud
    EVSP_send_ack();

    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        LOG_MSG_APPEND(log_content, "EVSP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            LOG_MSG_APPEND(log_content, "%x ", read_buf.content[i]);
        }
        LOG_MSG_APPEND(log_content, "\n");
    }

    LOG_MSG_APPEND(log_content, "EVSP cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0: {  // disable/enable:1/2
        uint8_t enableOrdisable = 0;
        read_int8_t(&enableOrdisable, &read_buf);
        if (enableOrdisable == 1 &&
            EVSP.dontSend2TC == 0) {  // enable/clear command buffer
            EVSP.dontSend2TC = 1;
            LOG_MSG_INFO("evsp disable");
        } else if (enableOrdisable == 2 &&
                   EVSP.dontSend2TC ==
                       1) {  // disable command buffer/then stop the command in
                             // command buffer sent to tc machine
            EVSP.dontSend2TC = 0;
            command_buf_clear();
            LOG_MSG_INFO("evsp enable and command buffer clear");
        } else {
            LOG_MSG_INFO(
                "invalid cloud pcket disable/enable packet to tc machine\n");
        }        
    } break;
    
    default:
        break;
    }

    LOG_MSG_INFO(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

#define insert_command_and_log                                      \
    do {                                                            \
        ret = command_buf_insert_effect_time(&command);             \
        LOG_MSG_APPEND(log_content,                                 \
                "\ncycle: %d, phase: %d, effect time: %d (%d)",     \
                command.cycle, command.phase, command.effect_time,  \
                ret);                                               \
    } while (0);

int static inline EVSP_rolling_to_target_phase(int target_phase, char *OBU_name, char *log_content, traffic_signal_status_t *signal_status)
{
    EVSP_host_OBU_obj_print();
    tsc_command_t command;
    memset(&command, 0, sizeof(tsc_command_t));
    command.app_id = EVSP.id;
    command.app_priority = EVSP.priority;
    command.target_phase = target_phase;
    strncpy(command.host_OBU_name, OBU_name, OBU_NAME_MAX_LEN);

    uint8_t current_phase = signal_status->SubPhaseID;
    uint8_t current_step = signal_status->StepID;
    uint16_t current_second = signal_status->StepSec;

    uint16_t pretime = signal_status->plan[target_phase - 1].PreTimeCompensated;
    int ret = 0;
    int16_t EVSP_adjust_time = 0;

    EVSP_adjust_time = EVSP_extend_formula(target_phase, signal_status, 20);  // Gmx += 20 ，緩衝誤差值調最大
    LOG_MSG_TRACE("new EVSP_adjust_time:%d", EVSP_adjust_time);
    // 因為只有 step 1 綠燈可以動態控制 所以不是在綠燈的時候就當作到下個時相了
    if (current_step != 1)
        current_phase++;

    command.cycle = 0;  // 0 代表線在這個 cycle

    // 如果 current_phase >= target_phase，i 就會加到 target_phase
    // target_phase < current_phase，的話就會停在 SubPhaseCount 把現在的 cycle 都換成最小綠
    for (int i = current_phase; i <= signal_status->SubPhaseCount && i != target_phase; i++) {
        command.phase = i;
        command.effect_time = EVSP_config.min_green;  // 縮短到最小綠
        insert_command_and_log;
    }

    /* target_phase >= current_phase */
    if (target_phase < current_phase) { /* target_phase < current_phase */
        // 如果 target_phase < current_phase 就代表在下一個 cycle
        command.cycle = 1;  // 所以這裡 cycle = 1 並下面再插入目標時向的時候舊式下一個 cycle
        for (int i = 1; i < target_phase; i++) {
            command.phase = i;
            command.effect_time = EVSP_config.min_green;
            insert_command_and_log;
        }
    }

    // 如果同時有兩台緊急車輛採到觸碰區域，會進行綠燈延長的是先來的那台
    command.phase = target_phase;
    command.effect_time = pretime + EVSP_adjust_time;
    insert_command_and_log;
    return ret;
}

static void VMS_activate(int direction, vehicle_type_t vehicle_type)
{
    /** TODO
     * 目前的 EVSP VMS/CMS Service 受限於時間及沒有足夠的地理資訊，
     * 所以採取寫死的方案去 mapping 車輛的 direction 和 VMS 編號。
     * 未來如果有新增讓每一個 touching area 歸屬到一條道路的資訊，
     * 那再改寫成更 general 的設計。
     */
    
    /**
     * 245~254 EVSP 救護車 Reserved，以 245 為北順時針遞增編號
     * 235~244 EVSP 消防車 Reserved，以 235 為北順時針遞增編號
     * 225~234 EVSP 警車 Reserved，以 225 為北順時針遞增編號
     * 255 黑色
     */
    if (config.cms_number != 0) {
        uint8_t buf[CMS_NUM_MAX] = {0};
        switch (vehicle_type)
        {
            case VEHICLE_AMBULANCE:   
                if (direction == 7 || direction == 0) {
                    buf[0] = 245;
                    buf[1] = 246;
                    buf[2] = 247;
                    buf[3] = 248;
                } else if (direction == 1 || direction == 2) {
                    buf[0] = 248;
                    buf[1] = 245;
                    buf[2] = 246;
                    buf[3] = 247;
                } else if (direction == 3 || direction == 4) {
                    buf[0] = 247;
                    buf[1] = 248;
                    buf[2] = 245;
                    buf[3] = 246;
                } else if (direction == 5 || direction == 6) {
                    buf[0] = 246;
                    buf[1] = 247;
                    buf[2] = 248;
                    buf[3] = 245;
                }
                break;

            case VEHICLE_FIRE_TRUCK:
                if (direction == 7 || direction == 0) {
                    buf[0] = 235;
                    buf[1] = 236;
                    buf[2] = 237;
                    buf[3] = 238;
                } else if (direction == 1 || direction == 2) {
                    buf[0] = 238;
                    buf[1] = 235;
                    buf[2] = 236;
                    buf[3] = 237;
                } else if (direction == 3 || direction == 4) {
                    buf[0] = 237;
                    buf[1] = 238;
                    buf[2] = 235;
                    buf[3] = 236;
                } else if (direction == 5 || direction == 6) {
                    buf[0] = 236;
                    buf[1] = 237;
                    buf[2] = 238;
                    buf[3] = 235;
                }
                break;

            case VEHICLE_POLICE_CAR:
                if (direction == 7 || direction == 0) {
                    buf[0] = 225;
                    buf[1] = 226;
                    buf[2] = 227;
                    buf[3] = 228;
                } else if (direction == 1 || direction == 2) {
                    buf[0] = 228;
                    buf[1] = 225;
                    buf[2] = 226;
                    buf[3] = 227;
                } else if (direction == 3 || direction == 4) {
                    buf[0] = 227;
                    buf[1] = 228;
                    buf[2] = 225;
                    buf[3] = 226;
                } else if (direction == 5 || direction == 6) {
                    buf[0] = 226;
                    buf[1] = 227;
                    buf[2] = 228;
                    buf[3] = 225;
                }
                break;

            default:
                LOG_MSG_INFO("VMS_activate(): get wrong vehicle_type: %d", vehicle_type);
                return;
        }
        LOG_MSG_INFO("VMS_activate(): vehicle_type = %d, CMS buffer = %d %d %d %d", vehicle_type, buf[0], buf[1], buf[2], buf[3]);
        CMS_request_start(EVSP.id, EVSP.priority, buf);
    } else {
        /**
         * 1.根據方向更改 evsp_prog[]
         * 2.呼叫VMS SERVICE
         */
        memset(evsp_prog, 255, sizeof(evsp_prog));

        if (direction == 7 || direction == 0) {
            // evsp_prog = [245, 246, 247, 248, 0, 0, 0, 0];
            evsp_prog[1] = 245;
            evsp_prog[0] = 246;
            evsp_prog[3] = 247;
            evsp_prog[2] = 248;
        } else if (direction == 1 || direction == 2) {
            // evsp_prog = [248, 245, 246, 247, 0, 0, 0, 0];
            evsp_prog[1] = 248;
            evsp_prog[0] = 245;
            evsp_prog[3] = 246;
            evsp_prog[2] = 247;
        } else if (direction == 3 || direction == 4) {
            // evsp_prog = [247, 248, 245, 246, 0, 0, 0, 0];
            evsp_prog[1] = 247;
            evsp_prog[0] = 248;
            evsp_prog[3] = 245;
            evsp_prog[2] = 246;
        } else if (direction == 5 || direction == 6) {
            // evsp_prog = [246, 247, 248, 245, 0, 0, 0, 0];
            evsp_prog[1] = 246;
            evsp_prog[0] = 247;
            evsp_prog[3] = 248;
            evsp_prog[2] = 245;
        }

        vms_request_start(EVSP.id, EVSP.priority);
    }
}

int EVSP_on_OBU_packet_rx(void *arg)
{
    V2R_app_section_t *app_section = (V2R_app_section_t *) arg;
    if (app_section->OBU_object->vehicle_type != VEHICLE_AMBULANCE 
    &&  app_section->OBU_object->vehicle_type != VEHICLE_FIRE_TRUCK
    &&  app_section->OBU_object->vehicle_type != VEHICLE_POLICE_CAR) {
        return 0;
    }
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));

    /* If SRM checks whether this message is for self. */
    if (app_section->msgID == SignalRequestMessage_Id) {
        SignalRequestMessage *srm = app_section->data;
        if (srm->requests_option) {
            int i = 0;
            for (i; i < srm->requests.count; i++) {
                if (srm->requests.tab[i].request.id.id == config.RSU_id)
                    break;
            }
            if (i == srm->requests.count) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    Malloc(read_buf.content, app_section->payload_len, "EVSP_on_OBU_packet_rx");

    memcpy(read_buf.content, app_section->payload, app_section->payload_len);

    /* print packet */
    if (config.log_OBU_packet_rx) {
        LOG_MSG_APPEND(log_content, "EVSP OBU packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            LOG_MSG_APPEND(log_content, "%x ", read_buf.content[i]);
        }
        LOG_MSG_APPEND(log_content, "\n");
    }

    EVSP_static_space_t static_space;
    memcpy(&static_space, app_section->OBU_object->private_space->static_space, sizeof(EVSP_static_space_t));
    read_uint8_t(&static_space.on_duty_flag, &read_buf);
    read_uint8_t(&static_space.weight, &read_buf);
    read_uint8_t(&static_space.error_code, &read_buf);


    uint8_t last_record_index =
        app_section->OBU_object->record_ring
            .last_record_pointer;  // back of queue; 最新推入的資料？

    // 轉傳緊急封包到雲端 (測試模式下註解掉)
    EVSP_report_host_obu(app_section->OBU_object, static_space.on_duty_flag);

    // obu與rsu的距離
    float OBU_lat = app_section->OBU_object->record_ring.record[last_record_index].position_lat;
    float OBU_lon = app_section->OBU_object->record_ring.record[last_record_index].position_lon;
    uint16_t OBU_distance = (uint16_t) get_distance(config.RSU_lat, config.RSU_lon, OBU_lat, OBU_lon);

    LOG_MSG_APPEND(log_content,
                "OBU name = %s\n"
                 "EVSP OBU packet rx: OBU POSITION\nlat, lon: %f, %f\n"
                 "OBU distance: %hd\nOBU direction: %hhd", app_section->OBU_object->OBU_name,
                 OBU_lat, OBU_lon, OBU_distance,
                 app_section->OBU_object->record_ring.record[last_record_index].direction);

    uint16_t last_record_distance = 0;

    // 有舊資料
    if (static_space.last_lon != 0 && static_space.last_lat != 0) {
        last_record_distance = (uint16_t) get_distance(
            static_space.last_lat, static_space.last_lon, OBU_lat, OBU_lon);
        LOG_MSG_APPEND(log_content,
                     "\nlast lat, last lon: %f, %f\nlast record distance: "
                     "%hd\nlast OBU direction: %hhd",
                     static_space.last_lat, static_space.last_lon,
                     last_record_distance, static_space.last_direction);
    }
    // 位移有超過閥值 才會紀錄下來 or 第一筆資料
    if (last_record_distance > EVSP_config.valid_record_distance || last_record_distance == 0) {
        static_space.last_lat = OBU_lat;
        static_space.last_lon = OBU_lon;
        static_space.last_direction = app_section->OBU_object->record_ring.record[last_record_index].direction;
    }
    memcpy(app_section->OBU_object->private_space->static_space, &static_space, sizeof(EVSP_static_space_t));

    LOG_MSG_INFO(log_content);
    memset(log_content, 0, sizeof(log_content));

    // 紀錄目前OBU位置
    EVSP_OBU_update_info_t update_info;
    update_info.lat = OBU_lat;
    update_info.lon = OBU_lon;
    update_info.direction = static_space.last_direction;
    update_info.speed = app_section->OBU_object->prediction_speed;
    update_info.is_activate = 0;
    update_info.vehicle_type = app_section->OBU_object->vehicle_type;
    // 確認接收的EVSP是否已在RSU紀錄中，如果沒有會新建立一份紀錄
    EVSP_host_OBU_obj_t *host_OBU = EVSP_host_OBU_obj_search(app_section->OBU_object->OBU_name, &update_info);
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        // LOG_MSG_TRACE("SubPhaseID is 0");
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return 0;
    }  // tc箱出現錯誤 直接不做

    /* already in host OBU list (thus already in activate area) */
    if (host_OBU != NULL) {
        // 新增觸發次數threshold判斷
        
        set_timer(host_OBU->host_OBU_packet_timer, 0, 0,
                  EVSP_config.evsp_host_obu_packet_timeout, 0);
        host_OBU->distance = OBU_distance;
        EVSP_terminate_area_t *area_ptr = EVSP_terminate(OBU_lon, OBU_lat, host_OBU->area_ptr);
        int ret = 0;
        // enter terminate area
        if (area_ptr != NULL) {
            LOG_MSG_APPEND(log_content, "EVSP OBU packet rx: TERMINATE\nOBU ID: %s\nterminate area id %d\n",
                         app_section->OBU_object->OBU_name, area_ptr->terminate_area_id);

            int target_phase = host_OBU->target_phase;
            /**
             * 0: terminate 的 host_OBU 不是 activate_OBU_head
             * 1: terminate 的 host_OBU 是 activate_OBU_head 且尚有其他 activate_OBU
             * 2: terminate 的 host_OBU 是 activate list 中的最後一台
            */
            int terminate_flag = EVSP_OBU_activation_time_end(host_OBU->OBU_name); 
            command_buf_delete_OBU(host_OBU->OBU_name);  // 刪除在 command buf 還沒下下去的指令，並減少路口已觸發EVSP數
            EVSP_cooling_list_insert(host_OBU->OBU_name, host_OBU->area_ptr); // 加入 cooling_list
            EVSP_host_OBU_obj_delete(host_OBU->OBU_name); //　從 host_OBU_list 中移除            
            LOG_MSG_APPEND(log_content,"Amount of active EVSPs left: %d\n",activate_amount);
            
            // no other host OBU with same target phase in host_OBU_list
            if (EVSP_host_OBU_obj_resume(target_phase) == true) { // resume 是如果有用到延長時間才需要做補償
                // 進行補償
                // 移到command_buffer_send執行，resume instruction 執行完才進行補償.
                command_buf_resume_control(EVSP.id);
        
            }
            // 發生terminate就關CMS
            if (terminate_flag != 0){
                LOG_MSG_APPEND(log_content, "CMS end\n");        
                if (config.cms_number != 0) {
                    CMS_request_end(EVSP.id);
                } else {
                    // 結束 EVSP_VMS_SERVICE
                    vms_request_end(EVSP.id);
                }
            }
            // 如果不是最後一台車就要再重啟
            if (terminate_flag == 1){
                EVSP_host_OBU_obj_t host_OBU_head = EVSP_get_host_OBU_head();
                LOG_MSG_APPEND(log_content, "activate head host terminated, change CMS display\n");
                if (host_OBU_head.OBU_name == 0) {
                    LOG_MSG_APPEND(log_content, "Error: terminate_flag = 1, but host_OBU_head = NULL\n");
                    CMS_request_end(EVSP.id);
                } else {
                    LOG_MSG_APPEND(log_content, "Change CMS display target to %s\n", host_OBU_head.OBU_name);
                    LOG_MSG_APPEND(log_content, "static last direction %d   host OBU head direction %d\n",static_space.last_direction, host_OBU_head.direction);
                    VMS_activate(host_OBU_head.direction, host_OBU_head.vehicle_type);
                }
            }
            else if (terminate_flag == 2)
                LOG_MSG_APPEND(log_content, "last activate head host terminated\n");
            // 回報碰到離開點 id
            EVSP_report_activate_area(app_section->OBU_object, TERMINATE_AREA, area_ptr->terminate_area_id);
        } 
        else { // is not in terminate area, but may be in activate area
            // 新增觸發次數threshold判斷
            LOG_MSG_APPEND(log_content, "OBU %s is not in terminate area\n", host_OBU->OBU_name);
            // todo(侑融): 同時有兩台救護車且同向行駛的處理方式
            if (host_OBU->is_activate==2) {
                LOG_MSG_APPEND(log_content, "OBU %s has been activated\n", host_OBU->OBU_name);
                // 判斷有無建立thread，有就直接return，
                // 沒有就把host_OBU資料copy到activate_OBU中並創造一條EVSP_OBU_activation_timer的thread
                EVSP_OBU_activation_timer_start(host_OBU);
            } else if (host_OBU->is_activate==1) {                
                uint8_t plan_id = signal_status.PlanID;
                EVSP_plan_table_t *plan = EVSP_plan_table_search(plan_id);
                if (plan == NULL) {
                    LOG_MSG_APPEND(log_content, "touching area plan not found\n");
                    LOG_MSG_INFO(log_content);
                    if (read_buf.content != NULL) {
                        free(read_buf.content);
                    }
                    return 0;
                }
                //更新activate status
                uint8_t target_phase = 0;
                EVSP_touching_area_t *area_ptr_touch = NULL;
                target_phase = EVSP_activate(
                    app_section->OBU_object->record_ring.record[last_record_index].position_lon,
                    app_section->OBU_object->record_ring.record[last_record_index].position_lat,
                    static_space.last_direction, plan, &area_ptr_touch);

                if (target_phase >= 1 && target_phase <= EVSP_PHASE_MAX) {
                    // 已滿足觸發次數條件，啟動OBU activation timer
                    if (host_OBU->touched_amount+1>=EVSP_config.touching_threshold) {
                        update_info.is_activate = 2;
                        update_info.is_touching = 1;
                        LOG_MSG_APPEND(log_content, "EVSP OBU packet rx: ACTIVATE\nOBU ID: %s\ntarget phase: %d\ntouching area id %d\n",
                            app_section->OBU_object->OBU_name, host_OBU->target_phase, area_ptr_touch->touching_area_id);
                        host_OBU = EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_name, target_phase, area_ptr_touch, &update_info);
                        // 標記該OBU為已觸發，並啟動相關timer
                        int ret = EVSP_OBU_activation_timer_start(host_OBU);
                        if (ret == 1) {
                            LOG_MSG_APPEND(log_content, "CMS activate\n");
                            VMS_activate(host_OBU->direction, host_OBU->vehicle_type);
                        }
                        
                        pthread_mutex_lock(&mutex_active_EVSP);
                        activate_amount++;
                        pthread_mutex_unlock(&mutex_active_EVSP);                        
                        LOG_MSG_APPEND(log_content,"Amount of current active EVSPs: %d\n",activate_amount);
                        EVSP_report_activate_area(app_section->OBU_object, TOUCHING_AREA, area_ptr_touch->touching_area_id);
                    }
                    // 未滿足觸發次數條件，紀錄觸發次數+1
                    else {
                        update_info.is_activate = 1;
                        update_info.is_touching = 1;
                        host_OBU = EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_name, target_phase, area_ptr_touch, &update_info);
                    }                    
                }
            }                
            else {
                LOG_MSG_APPEND(log_content, "OBU %s is not activated\n", host_OBU->OBU_name);
            }   
        }
    } 
    else { /* not in host OBU list */
        // search plan
        uint8_t plan_id = signal_status.PlanID;
        EVSP_plan_table_t *plan = EVSP_plan_table_search(plan_id);

        if (plan == NULL) {
            LOG_MSG_APPEND(log_content, "\ntouching area plan not found");
            LOG_MSG_INFO(log_content);
            if (read_buf.content != NULL) {
                free(read_buf.content);
            }
            return 0;
        }

        //檢查EVSP是否觸動到路口activate area
        uint8_t target_phase = 0;
        EVSP_touching_area_t *area_ptr = NULL;
        target_phase = EVSP_activate(
            app_section->OBU_object->record_ring.record[last_record_index].position_lon,
            app_section->OBU_object->record_ring.record[last_record_index].position_lat,
            static_space.last_direction, plan, &area_ptr);

        // 檢查 OBU name 與同方向是否有還在冷卻時間
        if (area_ptr != NULL && EVSP_cooling_list_search(app_section->OBU_object->OBU_name, area_ptr) > 0) {
            LOG_MSG_APPEND(log_content, "\nOBU %s is at touching area %d in cooling time",
                         app_section->OBU_object->OBU_name, area_ptr->touching_area_id);
            goto EVSP_OBU_PAKET_END;
        }

        // phase 的範圍是 1~8
        if (target_phase >= 1 && target_phase <= EVSP_PHASE_MAX) {
            LOG_MSG_APPEND(log_content, "new OBU %s\n", app_section->OBU_object->OBU_name);        
            //紀錄OBU狀態並將OBU設為pre_activate狀態
            update_info.is_touching = 1;
            update_info.is_activate = 1;
            host_OBU = EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_name, target_phase, area_ptr, &update_info);
            // 回報碰到觸碰點 id
            EVSP_report_activate_area(app_section->OBU_object, TOUCHING_AREA, area_ptr->touching_area_id);            
        }
    }

EVSP_OBU_PAKET_END:
    LOG_MSG_INFO(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

int EVSP_on_registration(void *arg)
{
    /* read evsp confile file*/
    int ret = EVSP_config_init();
    if (ret != EVSP_CONFIG_ACCEPT) {
        LOG_MSG_FATAL("error evsp reading config file: %d", ret);
    }

    if (EVSP_config.touching_area_config_type == EVSP_touching_area_DEFAULT)
        EVSP_default_config();
    else if (EVSP_config.touching_area_config_type == EVSP_touching_area_TABLE)
        EVSP_table_config();

    fflush(stdout);
    EVSP_plan_list_print();

    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, EVSP.name, EVSP.priority, SignalRequestMessage_Id, &EVSP_on_OBU_packet_rx);
    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, EVSP.name, EVSP.priority, BasicSafetyMessage_Id, &EVSP_on_OBU_packet_rx);

    return 0;
}