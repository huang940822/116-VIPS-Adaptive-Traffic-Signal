#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "byte_processing.h"
#include "com_packet_processing.h"
#include "config.h"
#include "error_status.h"
#include "log.h"
#include "network.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"
#include "vms.h"

wifi_adapter_device_t wifi_adapter;
pthread_mutex_t VMS_request_priority_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t VMS_program_update_thread_mutex = PTHREAD_MUTEX_INITIALIZER;

traffic_signal_status_t signal_status;
uint8_t rtm_phase[RTM_MAX] = {0};
char current_step[RTM_MAX];

int port_fd;

uint8_t request_priority;  // 應用層用vms_request_start()的方式來改，初始值為255(預設輪播)
uint8_t app_id;            // 表示為當前正在服務的對象，初始值為255(預設輪播)

uint8_t evsp_prog[RTM_MAX];  // 之後改成[246,247,248,249,0,0...]

// tx sequence format: (seq,p1,p2,p3,p4\n
// rx sequence format: (seq,reserve,programNo,location\n
char vms_packet_tx[VMS_PACKET_TX_LEN_MAX];
char vms_packet_rx[VMS_PACKET_RX_LEN_MAX];
char uint8_t_to_char[10];

uint8_t program_ids_green[RTM_MAX];
uint8_t program_ids_not_green[RTM_MAX];

int sequence_number;
int res;

uint8_t vms_respose_cnt[RTM_MAX];
int readCnt;

char *VMS_name[4] = {VMS_2, VMS_1, VMS_4, VMS_3};

//更新交通訊息
bool vms_program_update_thread_activate(uint8_t Program_ID, char *Program_Name)
{
    if (pthread_mutex_trylock(&VMS_program_update_thread_mutex) == 0) {
        VMS_update_args *args;
        Malloc(args, sizeof(VMS_update_args), "vms program update VMS_update_args");
        args->program_id = Program_ID;
        // 因為用另一個 thread 去 handle 所以需要把 name 多用一塊空間
        Malloc(args->program_name, strlen(Program_Name) + 1, "vms program update name");
        memcpy(args->program_name, Program_Name, strlen(Program_Name) + 1);

        pthread_t VMS_program_update_handler;
        int ret = pthread_create(&VMS_program_update_handler, NULL, VMS_program_update, args);
        if (ret != 0) {
            log_file_write_fatal_error("error creating VMS_program_update_handler: %d", ret);
            perror("vms: pthread_create");
            exit(errno);
        }
        return true;
    }
    return false;
}

void vms_request_start(uint8_t id, uint8_t priority)
{
    pthread_mutex_lock(&VMS_request_priority_mutex);
    if (priority < request_priority) {
        request_priority = priority;
        app_id = id;
    }
    pthread_mutex_unlock(&VMS_request_priority_mutex);
}

void vms_request_end(uint8_t id)
{
    pthread_mutex_lock(&VMS_request_priority_mutex);
    // 只有自己能關掉自己的服務，避免其他應用在 timeout 的時候把別人的 VMS service 關起來。
    if (app_id == id) {
        request_priority = CAROUSEL_NUM;
        app_id = CAROUSEL_NUM;
    }
    pthread_mutex_unlock(&VMS_request_priority_mutex);
}

int carousel_update(uint8_t VMS_ID, uint8_t Program_Type, uint8_t Program_ID)  // 雲端下了更新輪播，就要執行這個函數來更新輪播陣列
{
    // 寫入 program_id.txt
    FILE *input_file, *output_file;
    char search_str[100];  // 用於保存要查找的字符串
    char line[256];        // 用於保存每一行的內容

    memset(line, 0, sizeof(line));
    memset(search_str, 0, sizeof(search_str));

    if (Program_Type == 0) {                          // Green
        strcat(search_str, "PROGRAM_IDs_GREEN");      // 生成要查找的字符串
    } else {                                          // Not Green
        strcat(search_str, "PROGRAM_IDs_NOT_GREEN");  // 生成要查找的字符串
    }

    input_file = fopen(VMS_CONFIG_FILE, "r");
    output_file = fopen("./config/vms_config_after.txt", "w");

    if (input_file == NULL || output_file == NULL) {
        printf("carousel_update: Error opening vms_config.txt\n");
        log_file_write_fatal_error("carousel_update: Error opening vms_config.txt");
        if (input_file)
            fclose(input_file);
        if (output_file != NULL)
            fclose(output_file);
        return -4;
    }

    int flag = 0;
    // 找到要改寫的那一行寫入新內容
    while (fgets(line, sizeof(line), input_file) != NULL) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            char write_buf[256];
            memset(write_buf, 0, sizeof(write_buf));
            strcat(write_buf, search_str);
            for (int i = 0; i < RTM_MAX; i++) {
                if (i == VMS_ID) {
                    sprintf(uint8_t_to_char, "%d", Program_ID);
                } else {
                    if (Program_Type == 0) {  // Green
                        sprintf(uint8_t_to_char, "%d", vms_config.program_ids_green[i]);
                    } else {  // Not Green
                        sprintf(uint8_t_to_char, "%d", vms_config.program_ids_not_green[i]);
                    }
                }
                strcat(write_buf, SPACEBAR);
                strcat(write_buf, uint8_t_to_char);
            }
            strcat(write_buf, "\n");
            fprintf(output_file, "%s", write_buf);
            flag = 1;
        } else {
            fprintf(output_file, "%s", line);
        }
    }

    fclose(input_file);
    fclose(output_file);
    // 透過更改檔名的方式將修改後的文件取代原本的文件
    rename("./config/vms_config_after.txt", VMS_CONFIG_FILE);

    if (flag == 1) {
        printf("carousel_update: OverWrite vms_config.txt successful\n");
        log_file_write("carousel_update: OverWrite vms_config.txt successful");
    } else {
        printf("carousel_update: OverWrite vms_config.txt failed\n");
        log_file_write_fatal_error("carousel_update: OverWrite vms_config.txt failed");
    }

    if (VMS_ID > 7) {
        return -1;
    } else if (Program_Type != 0 && Program_Type != 1) {
        return -2;
    } else if (Program_ID == 0) {
        return -3;
    }

    if (Program_Type == 0) {  // Green
        vms_config.program_ids_green[VMS_ID] = Program_ID;
        log_file_write("program_ids_green[%d] change to %d", VMS_ID, Program_ID);
        printf("program_ids_green[%d] change to %d", VMS_ID, Program_ID);
    } else if (Program_Type == 1) {  // Not Green
        vms_config.program_ids_not_green[VMS_ID] = Program_ID;
        log_file_write("program_ids_not_green[%d] change to %d", VMS_ID, Program_ID);
        printf("program_ids_not_green[%d] change to %d", VMS_ID, Program_ID);
    }

    for (int i = 0, j = 0; i < RTM_MAX; i++) {
        if (vms_config.activate_directions[i] != 0) {
            program_ids_green[j] = vms_config.program_ids_green[i];
            program_ids_not_green[j] = vms_config.program_ids_not_green[i];
            j++;
        }
    }

    return 0;
}

void VMS_report_programs_id(uint8_t cmd)
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("VMS_report_programs_id: malloc");
        perror("VMS_report_programs_id: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(cmd, &write_buf);
    // Program IDs(Green)
    for (int i = 0; i < RTM_MAX; ++i) {
        write_uint8_t(vms_config.program_ids_green[i], &write_buf);
    }
    // Program IDs(Not Green)
    for (int i = 0; i < RTM_MAX; ++i) {
        write_uint8_t(vms_config.program_ids_not_green[i], &write_buf);
    }

    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);
    return;
}

void VMS_report_program_name(uint8_t cmd, uint8_t program_id)
{
    FILE *fp;
    char *pos;
    char line[256];      // 用於保存每一行的內容
    char search_str[8];  // 用於保存要查找的字符串
    char filename[100];

    memset(filename, 0, sizeof(filename));
    snprintf(search_str, sizeof(search_str), "%d ", program_id);  // 生成要查找的字符串

    fp = fopen(VMS_pic_database_path, "r+");

    if (fp == NULL) {
        log_file_write_fatal_error("VMS_report_programs_name: open %s failed", VMS_pic_database_path);
        return;
    }
    int flag = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            flag = 1;
            pos = strchr(line, ' ') + 1;
            if (pos != NULL) {
                strcpy(filename, pos);
            }
            break;
        }
    }

    if (flag == 0) {
        strcat(filename, "None\n");
    }

    fclose(fp);

    // 回傳給雲端
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("VMS_report_programs_name: malloc");
        perror("VMS_report_programs_name: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(cmd, &write_buf);
    // Program ID
    write_uint8_t(program_id, &write_buf);
    // Program Name, strlen(filename)-1 把換行字元刪掉
    filename[strlen(filename) - 1] = 0;
    write_char(filename, &write_buf, strlen(filename), PROGRAM_NAME_LEN);

    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);
    return;
}

int VMS_search_program(char *program_name)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(VMS_pic_path);  // 打開當前目錄
    if (dir == NULL) {
        printf("VMS_search_program: open directory failed\n");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {             // 讀取目錄中的每個檔案
        if (strcmp(entry->d_name, program_name) == 0) {  // 比較檔案名稱
            printf("%s 已經存在\n", program_name);
            closedir(dir);
            return 0;
        }
    }

    printf("%s 不存在\n", program_name);
    closedir(dir);
    return 1;
}

int VMS_wifi_connect(char *VMS_name)
{
    char wifi_connect_cmd[PROGRAM_UPLOAD_PACKET_LEN_MAX];
    memset(wifi_connect_cmd, 0, sizeof(wifi_connect_cmd));
    strcat(wifi_connect_cmd, WIFI_CONNECT_COMMAND_BEGIN);
    strcat(wifi_connect_cmd, SPACEBAR);
    strcat(wifi_connect_cmd, VMS_name);
    //strcat(wifi_connect_cmd, SPACEBAR);
    //strcat(wifi_connect_cmd, "password");
    //strcat(wifi_connect_cmd, SPACEBAR);
    //strcat(wifi_connect_cmd, VMS_WIFI_AP_PASSWORD);

    // printf("%s\n", wifi_connect_cmd);

    FILE *fp;
    char path[1024];

    /* 執行連線的指令並將輸出保存到 fp 中 */
    fp = popen(wifi_connect_cmd, "r");
    if (fp == NULL) {
        printf("VMS_wifi_connect: Failed to execute  Wi-Fi connect command\n");
        log_file_write_fatal_error("VMS_wifi_connect: Failed to execute  Wi-Fi connect command");
        pclose(fp);
        return -1;
    }

    int connected_flag = 0;
    char *pch;
    while (fgets(path, sizeof(path), fp) != NULL) {
        // printf("%s", path);
        pch = strstr(path, "Connection successfully");
        if (pch != NULL) {
            connected_flag = 1;
        }
    }

    if (connected_flag == 1) {
        printf("VMS_wifi_connect: Successfully connected Wi-Fi: %s\n", VMS_name);
        log_file_write("VMS_wifi_connect: Successfully connected %s", VMS_name);
    } else {
        printf("VMS_wifi_connect: Failed to connect Wi-Fi: %s\n", VMS_name);
        log_file_write_fatal_error("VMS_wifi_connect: Failed to connect Wi-Fi: %s", VMS_name);
        pclose(fp);
        return -2;
    }

    pclose(fp);

    system("sudo route add 192.168.10.222 dev wlx5c925ed425c8");

    return 0;
}

int VMS_wifi_disconnect()
{
    FILE *fp;
    char path[1024];

    char wifi_disconnect_cmd[PROGRAM_UPLOAD_PACKET_LEN_MAX];
    memset(wifi_disconnect_cmd, 0, sizeof(wifi_disconnect_cmd));
    strcat(wifi_disconnect_cmd, WIFI_DISCONNECT_COMMAND_BEGIN);
    strcat(wifi_disconnect_cmd, wifi_adapter.device_name);
    strcat(wifi_disconnect_cmd, WIFI_DISCONNECT_COMMAND_END);
    /* 執行斷線的指令並將輸出保存到 fp 中 */
    fp = popen(wifi_disconnect_cmd, "r");
    if (fp == NULL) {
        printf("VMS_wifi_disconnect: Failed to execute  Wi-Fi disconnect command\n");
        log_file_write_fatal_error("VMS_wifi_disconnect: Failed to execute  Wi-Fi disconnect command");
        pclose(fp);
        return -1;
    }

    int disconnected_flag = 0;
    char *pch1, *pch2;
    /* 從 fp 中讀資料並比對是不是本來就沒連線或是成功斷線，是的話將 disconnected_flag 設為 1 */
    while (fgets(path, sizeof(path), fp) != NULL) {
        // printf("%s", path);
        pch1 = strstr(path, "disconnecting failed: This device is not active");
        pch2 = strstr(path, "successfully disconnected");
        if (pch1 != NULL || pch2 != NULL) {
            disconnected_flag = 1;
        }
    }

    if (disconnected_flag == 1) {
        printf("VMS_wifi_disconnect: Successfully disconnected\n");
        log_file_write("VMS_wifi_disconnect: Successfully disconnected");
    } else {
        printf("VMS_wifi_disconnect: Failed to disconnect Wi-Fi\n");
        log_file_write_fatal_error("VMS_wifi_disconnect: Failed to disconnect Wi-Fi");
        pclose(fp);
        return -2;
    }

    pclose(fp);
    return 0;
}

int VMS_program_update_packet_tx(uint8_t program_id, char *program_name)
{
    char write_buf[PROGRAM_UPLOAD_PACKET_LEN_MAX];
    memset(write_buf, 0, sizeof(write_buf));
    strcat(write_buf, PROGRAM_UPLOAD_PACKET_BEGIN);
    strcat(write_buf, SPACEBAR);
    strcat(write_buf, VMS_PROGRAM_UPLOADER_PATH);
    strcat(write_buf, SPACEBAR);
    strcat(write_buf, DOUBLE_QUOTATION_MARKS);
    strcat(write_buf, VMS_pic_path);
    strcat(write_buf, program_name);
    strcat(write_buf, DOUBLE_QUOTATION_MARKS);
    strcat(write_buf, SPACEBAR);
    strcat(write_buf, DOUBLE_QUOTATION_MARKS);
    char uint8_t_to_char[10];
    sprintf(uint8_t_to_char, "%d", program_id);
    strcat(write_buf, uint8_t_to_char);
    strcat(write_buf, DOUBLE_QUOTATION_MARKS);
    printf("%s\n", write_buf);

    // 看輸出來決定return 什麼
    FILE *fp;
    char path[1024];

    /* 執行上傳並將輸出保存到 fp 中 */
    fp = popen(write_buf, "r");
    if (fp == NULL) {
        printf("VMS_program_update_packet_tx: Failed to execute  upload command\n");
        log_file_write_fatal_error("VMS_program_update_packet_tx: Failed to execute  upload command");
        pclose(fp);
        return -1;
    }

    int upload_flag = 0;
    char *pch;
    char upload_successful_msg[PROGRAM_UPLOAD_PACKET_LEN_MAX];
    memset(upload_successful_msg, 0, sizeof(upload_successful_msg));
    strcat(upload_successful_msg, "Upload Program");
    strcat(upload_successful_msg, SPACEBAR);
    sprintf(uint8_t_to_char, "%d", program_id);
    strcat(upload_successful_msg, uint8_t_to_char);
    strcat(upload_successful_msg, SPACEBAR);
    strcat(upload_successful_msg, "successful");
    // printf("%s\n", upload_successful_msg);
    while (fgets(path, sizeof(path), fp) != NULL) {
        printf("%s", path);
        log_file_write("VMS_program_update_packet_tx: Upload message: %s", path);
        pch = strstr(path, upload_successful_msg);
        if (pch != NULL) {
            upload_flag = 1;
        }
    }
    // 不管上傳失敗與否都把上傳程式資料夾內裡面的 programs 資料夾刪掉，才不會有上傳異常的問題。
    system(DELETE_UPLOAD_PROGRAMS_FILES);

    if (upload_flag == 1) {
        printf("VMS_program_update_packet_tx: Upload Program %d successful\n", program_id);
        log_file_write("VMS_program_update_packet_tx: Upload Program %d successful", program_id);
    } else {
        printf("VMS_program_update_packet_tx: Upload Program %d failed\n", program_id);
        log_file_write_fatal_error("VMS_program_update_packet_tx: Upload Program %d failed", program_id);
        pclose(fp);
        return -2;
    }

    pclose(fp);
    return 0;
}

void *VMS_program_update(void *data)
{
    VMS_update_args *tmp = (VMS_update_args *) (data);
    uint8_t program_id = tmp->program_id;
    char *program_name = tmp->program_name;
    free(data);

    // 寫入 program_id.txt
    FILE *input_file, *output_file;
    char search_str[8];  // 用於保存要查找的字符串
    char line[256];      // 用於保存每一行的內容

    memset(line, 0, sizeof(line));
    memset(search_str, 0, sizeof(search_str));
    snprintf(search_str, sizeof(search_str), "%d ", program_id);  // 生成要查找的字符串

    input_file = fopen(VMS_pic_database_path, "r");
    output_file = fopen(VMS_pic_path "program_id_after.txt", "w");

    if (input_file == NULL || output_file == NULL) {
        printf("VMS_program_update: Error opening program_id.txt\n");
        log_file_write_fatal_error("VMS_program_update: Error opening %s", VMS_pic_database_path);
        pthread_mutex_unlock(&VMS_program_update_thread_mutex);

        if (input_file != NULL)
            fclose(input_file);
        if (output_file != NULL)
            fclose(output_file);
        if (program_name)
            free(program_name);

        pthread_detach(pthread_self());
        return NULL;
    }

    int flag = 0;
    // 找到要改寫的那一行寫入新內容
    while (fgets(line, sizeof(line), input_file) != NULL) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            char write_buf[256];
            memset(write_buf, 0, sizeof(write_buf));
            sprintf(uint8_t_to_char, "%d", program_id);
            strcat(write_buf, uint8_t_to_char);
            strcat(write_buf, SPACEBAR);
            strncat(write_buf, program_name, PROGRAM_NAME_LEN);
            strcat(write_buf, "\n");
            fprintf(output_file, "%s", write_buf);
            flag = 1;
        } else {
            fprintf(output_file, "%s", line);
        }
    }

    fclose(input_file);
    fclose(output_file);
    // 透過更改檔名的方式將修改後的文件取代原本的文件
    rename(VMS_pic_path "program_id_after.txt", VMS_pic_database_path);

    if (flag == 1) {
        printf("VMS_program_update: OverWrite program_id.txt successful\n");
        log_file_write("VMS_program_update: OverWrite program_id.txt successful");
    } else {
        printf("VMS_program_update: OverWrite program_id.txt failed\n");
        log_file_write_fatal_error("VMS_program_update: OverWrite program_id.txt failed");
    }

    // 每個 VMS 有設定上傳失敗重新上傳的閾值(包含連不到 Wi-Fi)
    // 如果有一個 VMS 超過閾值都還沒上傳成功，則判斷上傳異常，拉起 VMS 異常的 bit
    // 計數方式是連不上wifi就+1，或是連上wifi但是上傳失敗就+1
    int res;
    // 無論有沒有連接都先斷線一次
    res = VMS_wifi_disconnect();

    if (res == 0) {
        uint8_t upload_error_cnt[RTM_MAX];
        memset(upload_error_cnt, 0, sizeof(upload_error_cnt));

        for (int i = 0; i < 4; i++) {
            while (upload_error_cnt[i] < VMS_RESEND_THRESHOLD) {
                sleep(1);
                res = VMS_wifi_connect(VMS_name[i]);
                if (res == 0) {
                    break;
                }
                upload_error_cnt[i]++;
            }
            // 表示成功連接 Wi-Fi，準備開始嘗試上傳 Program
            if (res == 0) {
                while (upload_error_cnt[i] < VMS_RESEND_THRESHOLD) {
                    sleep(1);
                    res = VMS_program_update_packet_tx(program_id, program_name);
                    if (res == 0) {
                        break;
                    }
                    upload_error_cnt[i]++;
                }
            }
            sleep(1);
            VMS_wifi_disconnect();
        }

        // 檢查是否有 VMS 異常
        for (int i = 0; i < 4; i++) {
            if (upload_error_cnt[i] >= VMS_RESEND_THRESHOLD) {
                // 認定 VMS 異常，回報給雲端
                set_vms_error();
                printf("VMS_program_update: VMS ID: %s error\n", VMS_name[i]);
                log_file_write_fatal_error("VMS_program_update: VMS ID: %s error", VMS_name[i]);
            }
        }
    }
    // 睡兩秒保證 vms_error 的資訊有送回雲端，然後再把 vms_error 清掉
    // 因為上傳不是常態性的操作，頻率很低，所以不能等到下次上傳沒有問題才把 vms_error 清掉
    // 但如果定期回報給雲端的時間超過兩秒的話就會有問題(是否有紀錄現在是多久回傳一次的變數存在?)
    sleep(2);
    clear_vms_error();
    pthread_mutex_unlock(&VMS_program_update_thread_mutex);
    if (program_name)
        free(program_name);

    // 回傳雲端上傳成功
    // 暫時使用與 TSP ack 相同的封包格式
    // cmd 9, status 0
    msg_buf_t write_buf;
    write_buf.index = 0;
    Malloc(write_buf.content, R2C_SPECIFIC_FIELD_MAX_LEN, "TSP_send_ack: malloc");

    // cmd
    write_uint8_t(9, &write_buf);
    write_uint8_t(0, &write_buf);

    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);

    pthread_detach(pthread_self());
}

void phase_rtm_connect()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    memset(rtm_phase, 0, sizeof(rtm_phase));
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for (int j = 0; j < signal_status.SignalCount && i < RTM_MAX; j++) {
            if ((signal_status.phaseorder_plan[i][j].SignalStatus & 0b00111100) > 0) {
                rtm_phase[j] |= (1 << i);
            }
        }
    }
}

void control_loop()
{
    sequence_number = (sequence_number + 1) % 256;
    if (sequence_number == 0) {
        sequence_number++;
    }

    // select
    /*while (1) {
        res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
        printf("Hello %d\n", res);

        if(res <= 0){
            break;
        }
        printf("vms_packet_rx: %s\n", vms_packet_rx);
    }*/

    get_traffic_signal_status(&signal_status);
    phase_rtm_connect();
    // printf("PhaseOrder %02x SubPhaseID %d StepID %d StepSec %d\n", signal_status.PhaseOrder, signal_status.SubPhaseID, signal_status.StepID, signal_status.StepSec);

    memset(vms_packet_rx, 0, sizeof(vms_packet_tx));

    strcpy(vms_packet_tx, VMS_PACKET_BEGIN);
    sprintf(uint8_t_to_char, "%d", sequence_number);
    strcat(vms_packet_tx, uint8_t_to_char);

    switch (app_id) {
    case EVSP_ID:  // EVSP
    {
        for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
            sprintf(uint8_t_to_char, "%d", evsp_prog[i]);
            strcat(vms_packet_tx, VMS_PACKET_COMMA);
            strcat(vms_packet_tx, uint8_t_to_char);
        }

    } break;
    case CAROUSEL_NUM: {
        uint8_t current_phase = 1 << (signal_status.SubPhaseID - 1);
        for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
            if ((rtm_phase[i] & current_phase) > 0 && signal_status.StepID <= 3) {  // Green
                current_step[i] = 'G';
            } else {  // Not Green
                current_step[i] = 'R';
            }
        }

        for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
            int j;
            if (i == 0) {
                j = 1;
            } else if (i == 1) {
                j = 0;
            } else if (i == 2) {
                j = 3;
            } else {
                j = 2;
            }
            if (current_step[j] == 'G') {
                sprintf(uint8_t_to_char, "%d", program_ids_green[j]);
            } else {
                sprintf(uint8_t_to_char, "%d", program_ids_not_green[j]);
            }
            strcat(vms_packet_tx, VMS_PACKET_COMMA);
            strcat(vms_packet_tx, uint8_t_to_char);
        }
    } break;
    default: {
        log_file_write("Useless vms app_id: %d", app_id);
    } break;
    }

    strcat(vms_packet_tx, VMS_PACKET_END);
    res = write(port_fd, vms_packet_tx, strlen(vms_packet_tx));
    if (res > 0) {
        log_file_write("vms_packet_tx: %s", vms_packet_tx);
        // printf("vms_packet_tx: %s", vms_packet_tx);
    }
    sleep(1);
    res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
    // res == -1 case(EAGAIN)
    if (res < 0) {
        // 處理timeout
        printf("RS232: EAGAIN\n");
    } else if (res > 0) {
        log_file_write("vms_packet_rx: %s", vms_packet_rx);
        // printf("%s\n", vms_packet_rx);
    }

    readCnt++;
    for (int i = 1; i < strlen(vms_packet_rx); i++) {
        if (vms_packet_rx[i] == '\n') {
            // 49 是因為 VMS 編號是從1開始 所以多減一
            vms_respose_cnt[vms_packet_rx[i - 1] - 49]++;
        }
    }

    // printf("vms_respose_cnt:");
    // for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
    //     printf("%d ", vms_respose_cnt[i]);
    // }
    // printf("\n");
    // 每傳送十次檢查一次有沒有VMS已經超過十秒沒有回應，有的話判定 VMS 異常
    // 因為有一塊板子被廠商拿走了，所以這段程式碼會一直觸發異常
    if (readCnt == VMS_ERROR_THRESHOLD) {
        int errorFlag = 0;
        for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
            if (vms_respose_cnt[i] == 0) {
                errorFlag = 1;
                log_file_write_fatal_error("VMS_id : %d no respose", i + 1);
            }
        }
        if (errorFlag == 1) {
            set_vms_error();
            log_file_write_fatal_error("VMS : respose error");
        } else {
            clear_vms_error();
        }

        readCnt = 0;
        memset(vms_respose_cnt, 0, sizeof(vms_respose_cnt));
    }
}

void WiFi_adapter_search ()
{
    FILE *fp;
    char path[1024];

    /* 執行上傳並將輸出保存到 fp 中 */
    fp = popen("nmcli dev status | grep wifi", "r");
    if (fp == NULL) {
        printf("WiFi_adapter_search: Failed to execute command\n");
        log_file_write_fatal_error("WiFi_adapter_search: Failed to execute command");
        pclose(fp);
        return;
    }
    char *pch;
    while (fgets(path, sizeof(path), fp) != NULL) {
        printf("%s", path);
        log_file_write("WiFi_adapter_search: search message: %s", path);
        pch = strstr(path, "wifi");
        if (pch != NULL) {
            strncpy(wifi_adapter.device_name, path, pch - path - 2);
            printf("%s %ld\n", wifi_adapter.device_name, strlen(wifi_adapter.device_name));
        }
    }
}

void vms_handler_init()
{   
    
    WiFi_adapter_search();

    memset(program_ids_green, 255, sizeof(program_ids_green));
    memset(program_ids_not_green, 255, sizeof(program_ids_not_green));
    for (int i = 0, j = 0; i < RTM_MAX; i++) {
        if (vms_config.activate_directions[i] != 0) {
            program_ids_green[j] = vms_config.program_ids_green[i];
            program_ids_not_green[j] = vms_config.program_ids_not_green[i];
            j++;
        }
    }

    printf("Green: ");
    for (int i = 0, j = 0; i < RTM_MAX; i++) {
        printf("%d ", program_ids_green[i]);
    }
    printf("\n");

    printf("Not Green: ");
    for (int i = 0, j = 0; i < RTM_MAX; i++) {
        printf("%d ", program_ids_not_green[i]);
    }
    printf("\n");

    srand(time(NULL));
    sequence_number = (rand() % CAROUSEL_NUM) + 1;
    readCnt = 0;
    memset(vms_respose_cnt, 0, sizeof(vms_respose_cnt));
    request_priority = CAROUSEL_NUM;
    app_id = CAROUSEL_NUM;

    port_fd = open(VMS_SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (port_fd == -1) {
        log_file_write_fatal_error("error opening %s", VMS_SERIAL_PORT);
    } else {
        log_file_write("%s opened successfully", VMS_SERIAL_PORT);
    }

    vms_set_serial_attribs();

    res = net_non_block("set vms port non block.", port_fd);

    // 需要做一次送編號全255的當作初始化，才不會IPC當機恢復之後因為 VMS timeout 所以沒辦法正常播放節目
    // 因為有一塊板子的wifi壞了，暫時沒辦法全部上傳黑色節目到編號255
    memset(vms_packet_rx, 0, sizeof(vms_packet_rx));
    memset(vms_packet_tx, 0, sizeof(vms_packet_tx));
    strcat(vms_packet_tx, VMS_PACKET_BEGIN);
    sprintf(uint8_t_to_char, "%d", sequence_number);
    strcat(vms_packet_tx, uint8_t_to_char);
    strcat(vms_packet_tx, ",255,255,255,255\n");
    res = write(port_fd, vms_packet_tx, strlen(vms_packet_tx));
    if (res > 0) {
        log_file_write("vms_packet_tx: %s", vms_packet_tx);
        // printf("vms_packet_tx: %s", vms_packet_tx);
    }
    sleep(1);
    res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
    if (res < 0) {
        // 處理異常
        printf("RS232: EAGAIN\n");
    } else if (res > 0) {
        log_file_write("vms_packet_rx: %s", vms_packet_rx);
        // printf("%s\n", vms_packet_rx);
    }
}

void vms_set_serial_attribs()
{
    struct termios serial_port_settings; /* Create the structure */

    tcgetattr(port_fd, &serial_port_settings); /* Get the current attributes of the Serial port */

    /* Setting the Baud rate */
    cfsetispeed(&serial_port_settings, VMS_BAUDRATE); /* Set Read  Speed as 9600 */
    cfsetospeed(&serial_port_settings, VMS_BAUDRATE); /* Set Write Speed as 9600 */

    /* 8N1 Mode */
    serial_port_settings.c_cflag &=
        ~PARENB; /* Disables the Parity Enable bit(PARENB),So No Parity   */
    serial_port_settings.c_cflag &=
        ~CSTOPB; /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
    serial_port_settings.c_cflag &=
        ~CSIZE;                          /* Clears the mask for setting the data size             */
    serial_port_settings.c_cflag |= CS8; /* Set the data bits = 8 */

    serial_port_settings.c_cflag |=
        CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */

    serial_port_settings.c_iflag =
        IGNPAR; /* Ignore framing errors and parity errors */
    serial_port_settings.c_oflag &=
        ~OPOST; /* No Output Processing					 */
    serial_port_settings.c_lflag &=
        ~(ICANON | ECHO | ECHOE | ISIG); /* Non Cannonical mode */

    /* Setting Time outs */
    serial_port_settings.c_cc[VMIN] =
        VMIN_LEN;                         /* Read at least 20 characters */
    serial_port_settings.c_cc[VTIME] = 0; /* Wait indefinetly            */

    /* Set the attributes to the termios structure */
    if ((tcsetattr(port_fd, TCSANOW, &serial_port_settings)) != 0) {
        log_file_write_fatal_error("error setting attributes of %s",
                                   VMS_SERIAL_PORT);
    } else {
        log_file_write("%s set attributes successfully", VMS_SERIAL_PORT);
    }
    sleep(2); /* required to make flush work, for some reason */
    tcflush(port_fd,
            TCIOFLUSH); /* Discards old data in the rx buffer 		  */
}

void *vms_handler()
{
    if (vms_config.vms_active == 0) {
        pthread_detach(pthread_self());
        return NULL;
    }

    vms_handler_init();

    while (1) {
        control_loop();
        sleep(1);
    }

    close(port_fd);
}
// 判斷號誌燈號方向的文件
// https://ncku365-my.sharepoint.com/:p:/g/personal/p76101160_ncku_edu_tw/EdZ5RSBI6kxKgc_KwgRGn-YBjgrhCRPNddsJdz9qVu0ZsQ?rtime=RSZUgDsE20g
