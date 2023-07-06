#include "MMP.h"

pthread_mutex_t file_writer = PTHREAD_MUTEX_INITIALIZER;

app_obj_t MMP = {
    .name = "MMP",
    .id = MMP_ID,
    .priority = 4,
    .on_OBU_packet_rx = NULL,
    .on_OBU_packet_tx = NULL,
    .on_RSU_packet_rx = NULL,
    .on_RSU_packet_tx = NULL,
    .on_cloud_packet_rx = &MMP_on_cloud_packet_rx,
    .on_cloud_packet_tx = NULL,
    .on_traffic_signal_command_tx = NULL,
    .on_registration = &MMP_on_registration,
    .next = NULL,
    .dontSend2TC = 1,
};

int MMP_on_cloud_packet_rx(void *arg)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    C2R_app_section_t *app_section = (C2R_app_section_t *) arg;

    msg_buf_t read_buf;
    read_buf.index = 0;
    read_buf.content = (unsigned char *) malloc(app_section->payload_len);
    if (read_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("MMP_on_cloud_packet_rx: malloc");
        perror("MMP_on_cloud_packet_rx: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memcpy(read_buf.content, app_section->payload,
               app_section->payload_len);
    }

    // read cmd
    uint8_t cmd;
    uint8_t ack_status = 0;
    read_uint8_t(&cmd, &read_buf);

    /* print packet into log */
    if (config.log_cloud_packet_rx) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content),
                 "MMP cloud packet rx: SPECIFIC FIELD\n");
        for (int i = 0; i < app_section->payload_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     read_buf.content[i]);
        }
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "\n");
        log_file_write(log_content);
    }

    switch (cmd) {
        case 1: 
        {
            // 雲端變更控制策略
            uint8_t strategy = 0;
            uint8_t cyclenumber = 0;
            uint8_t temp_weightpi;
            read_int8_t(&strategy, &read_buf);
            read_int8_t(&cyclenumber, &read_buf);
            int temp_weightci = 0;
            int total_weight = 0;
            float phase_weight[PHASE_COUNT_MAX_NUM];

            if (strategy == 2) {
                for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
                    read_int8_t(&temp_weightpi, &read_buf);
                    temp_weightci = temp_weightpi;
                    if (strategy == 2) {
                        total_weight = total_weight + temp_weightci;
                        phase_weight[i] = temp_weightci * 1.0;
                    }
                }
                if (total_weight == 100) {
                    clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
                    for (int i = 0; i < PHASE_COUNT_MAX_NUM; i++) {
                        {
                            config.phase_weight[i] = phase_weight[i];
                            snprintf(log_content + strlen(log_content),
                                    LOG_CONTENT_LEN - strlen(log_content),
                                    "\nChange phase weight in config");
                        }
                    }
                } else {
                    printf("warning : total phase weight is not 100\n");
                    set_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
                }
            }

            if (strategy < 4) {
                if (0 < cyclenumber && cyclenumber < 3) {
                    clear_CLOUD_PACKET_CHANGE_STRATEGY_2_PHASE_WEIGHT_ERR();
                    config.traffic_compensation_cycle_number = cyclenumber;
                    config.traffic_compensation_method = strategy;
                    pthread_mutex_lock(&file_writer);
                    FILE *outfile;
                    outfile = fopen("config/config.txt", "w");
                    if (outfile == NULL) {
                        snprintf(log_content + strlen(log_content),
                                LOG_CONTENT_LEN - strlen(log_content),
                                "\nWarning: error opening ./config/config.txt ");
                    } else {
                        fprintf(outfile, "RSU_NAME \"%s\"\n", config.RSU_name);
                        fprintf(outfile, "RSU_id %d\n", config.RSU_id);
                        fprintf(outfile, "RSU_region %d\n", config.RSU_region);
                        fprintf(outfile, "RSU_LAT %f\n", config.RSU_lat);
                        fprintf(outfile, "RSU_LON %f\n", config.RSU_lon);

                        if (config.signal_controller_manufacturer == 0) {
                            fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER cheng_long\n");
                        } else if (config.signal_controller_manufacturer == 1) {
                            fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER shan_zhu\n");
                        } else {
                            fprintf(outfile, "SIGNAL_CONTROLLER_MANUFACTURER shan_zhu_m\n");
                        }

                        if (config.signal_status_report_active == true) {
                            fprintf(outfile, "SIGNAL_STATUS_REPORT_ACTIVE yes\n");
                        } else {
                            fprintf(outfile, "SIGNAL_STATUS_REPORT_ACTIVE no\n");
                        }

                        if (config.signal_adjust_upper_bound_active == true) {
                            fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_ACTVE yes\n");
                        } else {
                            fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_ACTVE no\n");
                        }

                        if (config.signal_adjust_lower_bound_active == true) {
                            fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_ACTVE yes\n");
                        } else {
                            fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_ACTVE no\n");
                        }

                        fprintf(outfile, "SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE %.0f\n", config.signal_adjust_upper_bound_percentage);
                        fprintf(outfile, "SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE %.0f\n", config.signal_adjust_lower_bound_percentage);
                        fprintf(outfile, "TRAFFIC_COMPENSATION_METHOD %d\n", config.traffic_compensation_method);
                        if (config.traffic_compensation_cycle_number == 1) {
                            fprintf(outfile, "TRAFFIC_COMPENSATION_CYCLE_NUMBER 1\n");
                        } else {
                            fprintf(outfile, "TRAFFIC_COMPENSATION_CYCLE_NUMBER 2\n");
                        }
                        fprintf(outfile, "PHASE_WEIGHT %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f\n", config.phase_weight[0], config.phase_weight[1], config.phase_weight[2],
                                config.phase_weight[3], config.phase_weight[4], config.phase_weight[5], config.phase_weight[6], config.phase_weight[7]);
                        if (config.log_middleware_timer_event == true) {
                            fprintf(outfile, "LOG_MIDDLEWARE_TIMER_EVENT yes\n");
                        } else {
                            fprintf(outfile, "LOG_MIDDLEWARE_TIMER_EVENT no\n");
                        }
                        if (config.log_application_register_event == true) {
                            fprintf(outfile, "LOG_APPLICATION_REGISTER_EVENT yes\n");
                        } else {
                            fprintf(outfile, "LOG_APPLICATION_REGISTER_EVENT no\n");
                        }
                        if (config.log_command_buffer == true) {
                            fprintf(outfile, "LOG_COMMAND_BUFFER yes\n");
                        } else {
                            fprintf(outfile, "LOG_COMMAND_BUFFER no\n");
                        }
                        if (config.log_signal_packet_rx == true) {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_RX yes\n");
                        } else {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_RX no\n");
                        }
                        if (config.log_signal_packet_tx == true) {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_TX yes\n");
                        } else {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_TX no\n");
                        }
                        if (config.log_signal_packet_info == true) {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_INFO yes\n");
                        } else {
                            fprintf(outfile, "LOG_SIGNAL_PACKET_INFO no\n");
                        }
                        if (config.log_cloud_packet_rx == true) {
                            fprintf(outfile, "LOG_CLOUD_PACKET_RXA yes\n");
                        } else {
                            fprintf(outfile, "LOG_CLOUD_PACKET_RXA no\n");
                        }
                        if (config.log_cloud_packet_tx == true) {
                            fprintf(outfile, "LOG_CLOUD_PACKET_TX yes\n");
                        } else {
                            fprintf(outfile, "LOG_CLOUD_PACKET_TX no\n");
                        }
                        if (config.log_OBU_packet_rx == true) {
                            fprintf(outfile, "LOG_OBU_PACKET_RX yes\n");
                        } else {
                            fprintf(outfile, "LOG_OBU_PACKET_RX no\n");
                        }
                        if (config.log_OBU_packet_tx == true) {
                            fprintf(outfile, "LOG_OBU_PACKET_TX yes\n");
                        } else {
                            fprintf(outfile, "LOG_OBU_PACKET_TX no\n");
                        }
                        if (config.log_OBU_list == true) {
                            fprintf(outfile, "LOG_OBU_LIST yes\n");
                        } else {
                            fprintf(outfile, "LOG_OBU_LIST no\n");
                        }
                    }
                    fclose(outfile);
                    pthread_mutex_unlock(&file_writer);
                } else {
                    snprintf(log_content + strlen(log_content),
                            LOG_CONTENT_LEN - strlen(log_content),
                            "\ninvalid cloud packet strategy MMP packet to tc "
                            "machine of invalid cyclenum: %d",
                            cyclenumber);
                }
            } else {
                snprintf(log_content + strlen(log_content),
                        LOG_CONTENT_LEN - strlen(log_content),
                        "\ninvalid cloud packet strategy MMP packet to tc "
                        "machine of invalid strategy: %d",
                        strategy);
            }
            log_file_write(log_content);
        } break;
        case 2:  // 雲端更新 VMS 圖片(要考慮到錯誤回報)
        {
            // 1.檢查檔名是否存在於VMS_pic資料夾中，無跳2.，有跳3.
            // 2.沒有指定檔案的回報處理並結束
            // 3.輪流連線四台VMS並上傳，都上傳成功則走5，反之則走4(設計成會容許失敗重傳幾次，有時候會找不到wifi，但是平均三次內都可以成功)
            // 4.上傳異常回報處理並結束
            // 5.上傳成功回報並結束
            uint8_t Program_ID;
            char Program_Name[100] = {0};
            read_uint8_t(&Program_ID, &read_buf);
            read_char(Program_Name, &read_buf, PROGRAM_NAME_LEN);
            trim_space(Program_Name);
            int res;
            // 檢查檔案存不存在資料夾中
            res = VMS_search_program(Program_Name);

            switch (res) {
            case -1: {
                log_file_write_fatal_error("VMS_search_program: open directory failed");
            } break;
            case 0: {
                if (vms_program_update_thread_activate(Program_ID, Program_Name)) {
                    log_file_write("vms program update thread activate.");
                } else {  // 正在上傳
                    log_file_write_fatal_error("vms program update is process.");
                }
                ack_status = 2;
            } break;
            case 1: {
                log_file_write_fatal_error("VMS_search_program: program doesnt exist, program name = %s", Program_Name);
                ack_status = res;
            } break;
            }

        } break;
        case 3:  // 雲端更改 VMS 播放，設計成只有封包內容都正常才ACK
        {
            uint8_t VMS_ID;
            uint8_t Program_Type;
            uint8_t Program_ID;
            read_uint8_t(&VMS_ID, &read_buf);
            read_uint8_t(&Program_Type, &read_buf);
            read_uint8_t(&Program_ID, &read_buf);
            // printf("VMS_ID: %d, Program_Type: %d, Program_ID: %d\n", VMS_ID, Program_Type, Program_ID);
            log_file_write("VMS_ID: %d, Program_Type: %d, Program_ID: %d\n", VMS_ID, Program_Type, Program_ID);
            int res = carousel_update(VMS_ID, Program_Type, Program_ID);

            switch (res) {
            case 0:  // 正常
            {
                // ack_status = res;  // 初始值就是 0
            } break;
            case -1:  // VMS ID 有誤
            {
                log_file_write_fatal_error("Error VMS ID");
            } break;
            case -2:  // Program Type 有誤
            {
                log_file_write_fatal_error("Error Program Type");
            } break;
            case -3:  // Program ID 有誤
            {
                log_file_write_fatal_error("Error Program ID");
            } break;
            case -4:  // 無法開啟 vms_config.txt
            {
                log_file_write_fatal_error("Error opening vms_config.txt");
            } break;
            default: {
                log_file_write("Useless return value");
            } break;
            }

        } break;
        case 4:  // 雲端查詢 VMS 播放節目編號
        {
            VMS_report_programs_id(cmd);
        } break;
        case 5:  // 雲端查詢 VMS 編號對應檔案名稱
        {
            uint8_t program_id;
            read_uint8_t(&program_id, &read_buf);
            VMS_report_program_name(cmd, program_id);
        } break;
    }
}

int MMP_on_registration(void *arg)
{
    return 0;
}