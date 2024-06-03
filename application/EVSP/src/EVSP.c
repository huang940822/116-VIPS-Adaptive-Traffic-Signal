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
#include "traffic_signal_status_updating.h"
#include "vms.h"

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
    .dontSend2TC = 1,
};

int EVSP_on_CLOUD_packet_rx(void *arg)
{
    // printf("\nTSP_on_cloud_packet_rx function\n");
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    Malloc(read_buf.content, app_section->payload_len, "EVSP_on_cloud_packet_rx");
    if (read_buf.content == NULL)
        return -1;
    memcpy(read_buf.content, app_section->payload, app_section->payload_len);

    // needs a evsp sned ack function to send ack to cloud
    EVSP_send_ack();

    // read cmd
    uint8_t cmd;
    read_uint8_t(&cmd, &read_buf);

    /* print packet */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "EVSP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP cloud packet rx: CMD(%d)", cmd);

    switch (cmd) {
    case 0: {  // disable/enalbe:1/2
        uint8_t enableOrdisable = 0;
        // uint8_t type=0;
        read_int8_t(&enableOrdisable, &read_buf);
        // read_int8_t(&type, &read_buf);
        if (enableOrdisable == 1 &&
            EVSP.dontSend2TC == 0) {  // enable/clear command buffer
            EVSP.dontSend2TC = 1;
            log_file_write("evsp disable\r\n");
            printf("evsp disable\r\n");
        } else if (enableOrdisable == 2 &&
                   EVSP.dontSend2TC ==
                       1) {  // disable command buffer/then stop the command in
                             // command buffer sent to tc machine
            EVSP.dontSend2TC = 0;
            command_buf_clear();
            log_file_write("evsp enable and command buffer clear\r\n");
            printf("evsp enable\r\n");
        } else {
            log_file_write(
                "invalid cloud pcket disable/enable packet to tc machine\r\n");
        }
        // printf("not implement evsp on cloud rx action yet when cmd is
        // 0\r\n");
    } break;
    default:
        break;
    }

    log_file_write(log_content);
    if (read_buf.content != NULL) {
        free(read_buf.content);
    }
    return 0;
}

#define insert_command_and_log                                      \
    do {                                                            \
        ret = command_buf_insert_effect_time(&command);             \
        snprintf(log_content + strlen(log_content),                 \
                 LOG_CONTENT_LEN - strlen(log_content),             \
                 "\ncycle: %d, phase: %d, effect time: %d (%d)",    \
                 command.cycle, command.phase, command.effect_time, \
                 ret);                                              \
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
    printf("new EVSP_adjust_time:%d\n", EVSP_adjust_time);
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

static void VMS_activate(int direction)
{
    if (config.cms_number != 0) {
        uint8_t buf[CMS_NUM_MAX] = {0};
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
        CMS_request_start(EVSP.id, EVSP.priority, buf);
    } else {
        // 本案的 EVSP_VMS_SERVICE 受限於時間及沒有足夠的地理資訊，所以採取寫死的方案去 mapping 車輛的 direction 和 VMS 編號。
        // 未來如果有新增讓每一個 touching area 歸屬到一條道路的資訊，那再改寫成更 general 的設計。

        /*
        1.根據方向更改 evsp_prog[]
        2.呼叫VMS SERVICE
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
    if (app_section->OBU_object->vehicle_type != VEHICLE_AMBULANCE) {
        return 0;
    }
    // printf("EVSP_on_OBU_packet_rx function\n");
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
    Malloc(read_buf.content, app_section->payload_len, "EVSP_on_cloud_packet_rx");

    memcpy(read_buf.content, app_section->payload, app_section->payload_len);

    /* print packet */
    if (config.log_OBU_packet_rx) {
        log_snprintf(log_content, "EVSP OBU packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            log_snprintf(log_content, "%x ", read_buf.content[i]);
        }
        log_snprintf(log_content, "\n");
    }

    EVSP_static_space_t static_space;
    memcpy(&static_space, app_section->OBU_object->private_space->static_space, sizeof(EVSP_static_space_t));
    read_uint8_t(&static_space.on_duty_flag, &read_buf);
    read_uint8_t(&static_space.weight, &read_buf);
    read_uint8_t(&static_space.error_code, &read_buf);


    uint8_t last_record_index =
        app_section->OBU_object->record_ring
            .last_record_pointer;  // back of queue; 最新推入的資料？

    // 轉傳緊急封包到雲端
    EVSP_report_host_obu(app_section->OBU_object, static_space.on_duty_flag);

    // obu與rsu的距離
    float OBU_lat = app_section->OBU_object->record_ring.record[last_record_index].position_lat;
    float OBU_lon = app_section->OBU_object->record_ring.record[last_record_index].position_lon;
    uint16_t OBU_distance = (uint16_t) get_distance(config.RSU_lat, config.RSU_lon, OBU_lat, OBU_lon);

    log_snprintf(log_content,
                 "EVSP OBU packet rx: OBU POSITION\nlat, lon: %f, %f\n"
                 "OBU distance: %hd\nOBU direction: %hhd",
                 OBU_lat, OBU_lon, OBU_distance,
                 app_section->OBU_object->record_ring.record[last_record_index].direction);

    uint16_t last_record_distance = 0;

    // 有舊資料
    if (static_space.last_lon != 0 && static_space.last_lat != 0) {
        last_record_distance = (uint16_t) get_distance(
            static_space.last_lat, static_space.last_lon, OBU_lat, OBU_lon);
        log_snprintf(log_content,
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

    log_file_write(log_content);
    memset(log_content, 0, sizeof(log_content));

    EVSP_OBU_update_info_t update_info;
    update_info.lat = OBU_lat;
    update_info.lon = OBU_lon;
    update_info.direction = static_space.last_direction;
    update_info.speed = app_section->OBU_object->prediction_speed;
    EVSP_host_OBU_obj_t *host_OBU = EVSP_host_OBU_obj_search(app_section->OBU_object->OBU_name, &update_info);

    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    // for some error situation happens in CHENG_LONG
    if (signal_status.SubPhaseID == 0) {
        // printf("SubPhaseID is 0\n");
        if (read_buf.content != NULL) {
            free(read_buf.content);
        }
        return 0;
    }  // tc箱出現錯誤 直接不做

    /* already in host OBU list */
    if (host_OBU != NULL) {
        set_timer(host_OBU->host_OBU_packet_timer, 0, 0,
                  EVSP_config.evsp_host_obu_packet_timeout, 0);
        host_OBU->distance = OBU_distance;
        EVSP_terminate_area_t *area_ptr = EVSP_terminate(OBU_lon, OBU_lat, host_OBU->area_ptr);
        int ret = 0;
        // enter terminate area
        if (area_ptr != NULL) {
            log_snprintf(log_content, "EVSP OBU packet rx: TERMINATE\nOBU ID: %s\nterminate area id %d",
                         app_section->OBU_object->OBU_name, area_ptr->terminate_area_id);

            int target_phase = host_OBU->target_phase;
            EVSP_OBU_activation_time_end(host_OBU->OBU_name);
            command_buf_delete_OBU(host_OBU->OBU_name);  // 刪除在 command buf 還沒下下去的指令
            EVSP_cooling_list_insert(host_OBU->OBU_name, host_OBU->area_ptr);
            EVSP_host_OBU_obj_delete(host_OBU->OBU_name);

            // no other host OBU with same target phase in host_OBU_list
            if (EVSP_host_OBU_obj_resume(target_phase) == true) {
                // 進行補償
                // 移到command_buffer_send執行，resume instruction 執行完才進行補償.
                command_buf_resume_control(EVSP.id);

                if (config.cms_number != 0) {
                    CMS_request_end(EVSP.id);
                } else {
                    // 結束 EVSP_VMS_SERVICE
                    vms_request_end(EVSP.id);
                }
            }
            // 回報碰到觸碰點 id
            EVSP_report_activate_area(app_section->OBU_object, TERMINATE_ATRA, area_ptr->terminate_area_id);
        } else {
            // 同時有兩台救護車
            EVSP_OBU_activation_timer_start(host_OBU);
        }
    } else { /* not in host OBU list */
        // search plan
        uint8_t plan_id = signal_status.PlanID;
        EVSP_plan_table_t *plan = EVSP_plan_table_search(plan_id);

        if (plan == NULL) {
            log_snprintf(log_content, "\ntouching area plan not found");
            printf("touching area plan not found\n");
            log_file_write(log_content);
            if (read_buf.content != NULL) {
                free(read_buf.content);
            }
            return 0;
        }

        uint8_t target_phase = 0;
        EVSP_touching_area_t *area_ptr = NULL;
        target_phase = EVSP_activate(
            app_section->OBU_object->record_ring.record[last_record_index].position_lon,
            app_section->OBU_object->record_ring.record[last_record_index].position_lat,
            static_space.last_direction, plan, &area_ptr);

        // 檢查 OBU name 與同方向是否有還在冷卻時間
        if (area_ptr != NULL && EVSP_cooling_list_sreach(app_section->OBU_object->OBU_name, area_ptr) > 0) {
            printf("\nOBU %s is at touching area %d in cooling time\n",
                   app_section->OBU_object->OBU_name, area_ptr->touching_area_id);
            log_snprintf(log_content, "\nOBU %s is at touching area %d in cooling time",
                         app_section->OBU_object->OBU_name, area_ptr->touching_area_id);
            goto EVSP_OBU_PAKET_END;
        }
        // enter activate area
        // phase 的範圍是 1~8
        if (target_phase >= 1 && target_phase <= EVSP_PHASE_MAX) {
            log_snprintf(log_content, "EVSP OBU packet rx: ACTIVATE\nOBU ID: %s\ntarget phase: %d\ntouching area id %d",
                         app_section->OBU_object->OBU_name, target_phase, area_ptr->touching_area_id);

            printf("EVSP_activate SubPhaseID %d touching_area_Id %d ---\n", target_phase, area_ptr->touching_area_id);

            host_OBU = EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_name, target_phase, area_ptr, &update_info);

            int ret = EVSP_OBU_activation_timer_start(host_OBU);
            if (ret != -1) {
                VMS_activate(static_space.last_direction);
            }
            // 回報碰到觸碰點 id
            EVSP_report_activate_area(app_section->OBU_object, TOUCHING_AREA, area_ptr->touching_area_id);
        }
    }

EVSP_OBU_PAKET_END:
    log_file_write(log_content);
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
        log_file_write_fatal_error("error evsp reading config file: %d", ret);
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
