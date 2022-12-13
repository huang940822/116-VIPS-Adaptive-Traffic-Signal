#include <dirent.h>
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
#include "EVSP_timer_event.h"
#include "EVSP_touching_area.h"
#include "EVSP_typedefine.h"
#include "application_registration.h"
#include "byte_processing.h"
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

app_obj_t EVSP = {
    .name = "EVSP",
    .id = 1,
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
    // typedef struct C2R_app_section {
    //     uint32_t payload_len;
    //     char *payload;
    //     uint8_t com_id;
    // } C2R_app_section_t;
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

int EVSP_on_OBU_packet_rx(void *arg)
{
    V2R_app_section_t *app_section = (V2R_app_section_t *) arg;
    if (app_section->OBU_object->vehicle_type != VEHICLE_AMBULANCE)
        return 0;

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
            if (i == srm->requests.count)
                return -1;
        } else {
            return -1;
        }
    }

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    Malloc(read_buf.content, app_section->payload_len, "EVSP_on_cloud_packet_rx");
    if (read_buf.content == NULL) {
        return -1;
    }
    memcpy(read_buf.content, app_section->payload, app_section->payload_len);

    /* print packet */
    if (config.log_OBU_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "EVSP OBU packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
    }

    EVSP_static_space_t static_space;
    memcpy(&static_space,
           app_section->OBU_object->private_space->static_space,
           sizeof(EVSP_static_space_t));
    read_uint8_t(&static_space.on_duty_flag, &read_buf);
    read_uint8_t(&static_space.weight, &read_buf);
    read_uint8_t(&static_space.error_code, &read_buf);


    uint8_t last_record_index =
        app_section->OBU_object->record_ring
            .last_record_pointer;  // back of queue; 最新推入的資料？

    // 轉傳緊急封包到雲端

    {  // it's for evsp service's rx and try to get it's duty status and route
        // it to cloud
        // printf("recv special obu object and will route it to cloud for it's
        // evsp packet\n\r");

        msg_buf_t write_buf;
        write_buf.index = 0;
        write_buf.content = (unsigned char *) malloc(42);
        if (write_buf.content == NULL) {
            set_memory_error();
            log_file_write_fatal_error("OBU_packet_tx: malloc");
            perror("OBU_packet_tx: malloc");
            exit(errno);
        } else {
            clear_memory_error();
            // clear mem content which is malloced
            memset(write_buf.content, 0, 42);
        }
        write_uint8_t(0, &write_buf);  // write cmd
        write_char(
            app_section->OBU_object->OBU_name,
            &write_buf, OBU_NAME_MAX_LEN, OBU_NAME_MAX_LEN);  // write OBU_name
        write_uint8_t(
            app_section->OBU_object->vehicle_type,
            &write_buf);  // write vehicle_type

        char timestamp_t[20];
        struct tm *timeinfo;
        timeinfo = localtime(&app_section->OBU_object->record_ring
                                  .record[last_record_index]
                                  .time_second);
        int length =
            strftime(timestamp_t, 20, "%Y-%m-%d %H:%M:%S\n", timeinfo);
        // printf("obu object timestamp string is %s\n\r",timestamp_t);
        write_char(timestamp_t, &write_buf, TIMESTAMP_LEN,
                   TIMESTAMP_LEN);  // write timestamp
        write_float(
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lon,
            &write_buf);  // write lon
        write_float(
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lat,
            &write_buf);  // write lat

        // printf("gps data %f
        // %f\r\n",app_section->OBU_object->record_ring.record[last_record_index].position_lon,
        // app_section->OBU_object->record_ring.record[last_record_index].position_lat);

        write_uint8_t(
            app_section->OBU_object->record_ring.record[last_record_index]
                .speed,
            &write_buf);  // write speed
        write_uint8_t(
            app_section->OBU_object->record_ring.record[last_record_index]
                .direction,
            &write_buf);  // write direction
        // printf("direction:%d\n\r",app_section->OBU_object->record_ring.record[last_record_index].direction);

        // write dummy duty
        // write_uint8_t(1, &write_buf);
        write_uint8_t(static_space.on_duty_flag,
                      &write_buf);  // write on_duty_flag
        // printf("on_duty_flag : %d\r\n",static_space.on_duty_flag);
        printf("route evsp to cloud\r\n");
        cloud_packet_tx(write_buf.index, EVSP.id, write_buf.content);
        free(write_buf.content);
    }

    // obu與rsu的距離
    uint16_t OBU_distance = (uint16_t) get_distance(
        config.RSU_lat, config.RSU_lon,
        app_section->OBU_object->record_ring.record[last_record_index]
            .position_lat,
        app_section->OBU_object->record_ring.record[last_record_index]
            .position_lon);


    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "EVSP OBU packet rx: OBU POSITION");
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "\nlat, lon: %f, %f\nOBU distance: %hd\nOBU direction: %hhd",
             app_section->OBU_object->record_ring.record[last_record_index]
                 .position_lat,
             app_section->OBU_object->record_ring.record[last_record_index]
                 .position_lon,
             OBU_distance,
             app_section->OBU_object->record_ring.record[last_record_index]
                 .direction);

    uint16_t last_record_distance = 0;

    // 有舊資料
    if (static_space.last_lon != 0 && static_space.last_lat != 0) {
        last_record_distance = (uint16_t) get_distance(
            static_space.last_lat, static_space.last_lon,
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lat,
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lon);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "\nlast lat, last lon: %f, %f\nlast record distance: "
                 "%hd\nlast OBU direction: %hhd",
                 static_space.last_lat, static_space.last_lon,
                 last_record_distance, static_space.last_direction);
    }
    // 位移有超過閥值 才會紀錄下來 or 第一筆資料
    if (last_record_distance > EVSP_config.valid_record_distance || last_record_distance == 0) {
        static_space.last_lon =
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lon;
        static_space.last_lat =
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lat;
        static_space.last_direction =
            app_section->OBU_object->record_ring.record[last_record_index]
                .direction;
    }
    memcpy(app_section->OBU_object->private_space->static_space,
           &static_space, sizeof(EVSP_static_space_t));
    log_file_write(log_content);

    EVSP_host_OBU_obj_t *host_OBU =
        EVSP_host_OBU_obj_search(app_section->OBU_object->OBU_name);

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

    memset(log_content, 0, sizeof(log_content));
    /* already in host OBU list */
    if (host_OBU != NULL) {
        set_timer(host_OBU->host_OBU_packet_timer, 0, 0,
                  EVSP_config.evsp_host_obu_packet_timeout, 0);
        host_OBU->distance = OBU_distance;
        uint8_t terminate = EVSP_terminate(
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lon,
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lat,
            host_OBU->area_ptr);
        int ret = 0;
        // enter terminate area
        if (terminate == true) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "EVSP OBU packet rx: TERMINATE");
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\nOBU ID: %s",
                     app_section->OBU_object->OBU_name);

            tsc_command_t command;
            memset(&command, 0, sizeof(tsc_command_t));
            command.app_id = EVSP.id;
            command.app_priority = EVSP.priority;
            command.target_phase = host_OBU->target_phase;
            strncpy(command.host_OBU_name, RESUME_ID, OBU_NAME_MAX_LEN);

            EVSP_host_OBU_obj_delete(app_section->OBU_object->OBU_name);

            // no other host OBU with same target phase in host_OBU_list
            if (EVSP_host_OBU_obj_resume(command.target_phase) == true) {
                uint8_t current_phase = signal_status.SubPhaseID;
                if (command.target_phase >= current_phase) {
                    command.cycle = 0;
                    command.phase = command.target_phase;
                    command.effect_time =
                        signal_status.plan[command.target_phase - 1]
                            .PreTimeCompensated;
                    ret = command_buf_insert_effect_time(&command);
                    // printf("RESUME cycle: %d, phase: %d, effect time: %d
                    // (%d)\r\n",command.cycle, command.phase,
                    // command.effect_time, ret);
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "\ncycle: %d, phase: %d, effect time: %d (%d)",
                             command.cycle, command.phase, command.effect_time,
                             ret);
                } else {  // target phase已過 到下一個cycle執行
                    command.cycle = 1;
                    command.phase = command.target_phase;
                    command.effect_time =
                        signal_status.plan[command.target_phase - 1]
                            .PreTimeCompensated;
                    ret = command_buf_insert_effect_time(&command);
                    // printf("RESUME cycle: %d, phase: %d, effect time: %d
                    // (%d)\r\n",command.cycle, command.phase,
                    // command.effect_time, ret);
                    snprintf(log_content + strlen(log_content),
                             LOG_CONTENT_LEN - strlen(log_content),
                             "\ncycle: %d, phase: %d, effect time: %d (%d)",
                             command.cycle, command.phase, command.effect_time,
                             ret);
                }
            }
            // 進行補償
            // 移到command_buffer_send執行，resume instruction 執行完才進行補償.
        }

    } else { /* not in host OBU list */

        // search plan
        uint8_t plan_id = get_plan_id();
        EVSP_touching_area_plan_list_t *plan =
            EVSP_touching_area_plan_search(plan_id);

        if (plan == NULL) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ntouching area plan not found");
            log_file_write(log_content);
            if (read_buf.content != NULL) {
                free(read_buf.content);
            }
            return 0;
        }

        uint8_t target_phase = 0;
        EVSP_touching_area_t *area_ptr = EVSP_activate(
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lon,
            app_section->OBU_object->record_ring.record[last_record_index]
                .position_lat,
            static_space.last_direction, &target_phase, plan);
        // enter activate area
        if (target_phase >= 0 && target_phase < EVSP_PHASE_MAX) {
            // printf("EVSP OBU packet rx: ACTIVATE\r\n");
            target_phase += 1;  // why +1  ??因為phase的值會在0~7但實際上會是1~8
            // printf("max green is %d\r\n", EVSP_config.max_green);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "EVSP OBU packet rx: ACTIVATE");
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "\nOBU ID: %s",
                     app_section->OBU_object->OBU_name);
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content),
                     "\ntarget phase: %d", target_phase);

            EVSP_host_OBU_obj_insert(app_section->OBU_object->OBU_name,
                                     target_phase, area_ptr);
            EVSP_host_OBU_obj_print();
            tsc_command_t command;
            memset(&command, 0, sizeof(tsc_command_t));
            command.app_id = EVSP.id;
            command.app_priority = EVSP.priority;
            command.target_phase = target_phase;
            strncpy(command.host_OBU_name, app_section->OBU_object->OBU_name,
                    OBU_NAME_MAX_LEN);

            uint8_t current_phase = signal_status.SubPhaseID;
            uint8_t current_step = signal_status.StepID;
            uint16_t current_second = signal_status.StepSec;

            uint16_t pretime =
                signal_status.plan[target_phase - 1].PreTimeCompensated;
            int ret = 0;
            int16_t EVSP_adjust_time = 0;

            // Ｇmx＝ΣＰnb＋Ｔbf
            // Ｇmx：延長最長綠燈秒數
            // ΣＰnb：非公車各分相最短綠及清道時間總和
            // Ｔbf：緩衝誤差值，約為１０-２０秒，視觸動之距離而調整
            // 路口號誌為三時相，週期Ｃ為１２０秒，第一時相為公車方向６４秒綠燈、４秒黃燈、２秒全紅；
            // 第二時相１０秒綠燈、３秒黃燈、２秒全紅；第三時相２９秒綠燈、３秒黃燈、３秒全紅；而第二時相最短綠為５秒、第三時相最短綠為１５秒
            // Ｇmx＝【（５＋３＋２）＋（１５＋３＋３）】

            for (int i = 1; i < 8; i++) {
                if (i != target_phase)
                    EVSP_adjust_time += signal_status.plan[i - 1].MinGreen +
                                        signal_status.plan[i - 1].Yellow +
                                        signal_status.plan[i - 1].AllRed;
            }
            EVSP_adjust_time += 20;  // Gmx += 20 ，緩衝誤差值調最大
            printf("new EVSP_adjust_time:%d\n", EVSP_adjust_time);

            // 因為只有 step 1 綠燈可以動態控制 所以不是在綠燈的時候就當作到下個時相了
            if (current_step != 1)
                current_phase++;

            #define insert_command_and_log \
                do { \
                ret = command_buf_insert_effect_time(&command); \
                snprintf(log_content + strlen(log_content), \
                         LOG_CONTENT_LEN - strlen(log_content), \
                         "\ncycle: %d, phase: %d, effect time: %d (%d)", \
                         command.cycle, command.phase, command.effect_time, \
                         ret); \
                } while (0);

            command.cycle = 0; // 0 代表線在這個 cycle

            command.effect_time = EVSP_config.min_green; // 縮短到最小綠

            // 如果 current_phase >= target_phase，i 就會加到 target_phase
            // target_phase < current_phase，的話就會停在 SubPhaseCount 把現在的 cycle 都換成最小綠
            for (int i = current_phase; i <= signal_status.SubPhaseCount && i != target_phase; i++) {
                command.phase = i;
                insert_command_and_log;
            }

            /* target_phase >= current_phase */
            if (target_phase >= current_phase) {
                command.phase = target_phase;
                command.effect_time = pretime + EVSP_adjust_time;
                insert_command_and_log;
            } else { /* target_phase < current_phase */
                command.cycle = 1; // 下一個 cycle
                command.effect_time = EVSP_config.min_green;
                for (int i = 1; i < target_phase; i++) {
                    command.phase = i;
                    insert_command_and_log;
                }
                
                command.phase = target_phase;
                command.effect_time = pretime + EVSP_adjust_time;
                insert_command_and_log;
            }
            #undef insert_command_and_log
        }
    }
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

    /* touching area */
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(TOUCHING_AREA_DIR)) == NULL) {
        log_file_write_fatal_error("error opening %s", TOUCHING_AREA_DIR);
    } else {
        log_file_write("%s opened successfully", TOUCHING_AREA_DIR);
    }

    char rsu_name[RSU_NAME_MAX_LEN];
    uint8_t plan_id;
    /* list all file */
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_type == 8) {
            /* parse file name */
            sscanf(dirp->d_name, "%[^_]_%hhd", rsu_name, &plan_id);
            if (strncmp(rsu_name, config.RSU_name, RSU_NAME_MAX_LEN) == 0) {
                EVSP_touching_area_plan_insert(dirp->d_name, plan_id);
            }
        }
    }
    fflush(stdout);
    closedir(dp);
    EVSP_touching_area_plan_print();

    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, EVSP.name, EVSP.priority, SignalRequestMessage_Id, &EVSP_on_OBU_packet_rx);
    event_callback_msg_id_insert(EVENT_OBU_PACKET_RX, EVSP.name, EVSP.priority, BasicSafetyMessage_Id, &EVSP_on_OBU_packet_rx);
    return 0;
}