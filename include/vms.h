#ifndef VMS_H
#define VMS_H

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include "time.h"

// 因為 fopen 解析不了~，所以要注意如果 usrname 不是 asrlab 的話要做對應的修正
#define VMS_pic_path "/home/asrlab/VMS_pic/"
#define VMS_pic_database_path VMS_pic_path "program_id.txt"

#define VMS_BAUDRATE B115200
#define VMS_SERIAL_PORT "/dev/ttyS1"
#define VMIN_LEN 20

// 一個路口最多有幾個方向
#define RTM_MAX 8

#define VMS_PACKET_TX_LEN_MAX 40
#define VMS_PACKET_RX_LEN_MAX 100
#define VMS_PACKET_BEGIN "("
#define VMS_PACKET_COMMA ","
#define VMS_PACKET_END "\n"

#define CAROUSEL_NUM 255
#define VMS_ERROR_THRESHOLD 10

#define VMS_1 "NCKU_1"
#define VMS_2 "NCKU_2"
#define VMS_3 "NCKU_3"
#define VMS_4 "NCKU_4"
#define VMS_WIFI_AP_PASSWORD "ncku_sqmbNVw2"
#define VMS_RESEND_THRESHOLD 20
#define VMS_WIFI_ADAPTER_NAME "wlxf42853198ed0"
#define FETCH_STDOUT_AND_STDERR "2>&1"

#define VMS_PROGRAM_UPLOADER_DIR "/home/asrlab/Desktop/CppNcku/"
#define VMS_PROGRAM_UPLOADER_PATH VMS_PROGRAM_UPLOADER_DIR "CppNcku.exe"
#define VMS_PROGRAM_UPLOADER_LOG_PATH VMS_PROGRAM_UPLOADER_DIR "upload.txt"
#define PROGRAM_UPLOAD_PACKET_BEGIN "wine"
#define DOUBLE_QUOTATION_MARKS "\""
#define SPACEBAR " "
#define PROGRAM_UPLOAD_PACKET_LEN_MAX 1000

#define WIFI_DISCONNECT_COMMAND "sudo nmcli dev disconnect" SPACEBAR VMS_WIFI_ADAPTER_NAME SPACEBAR FETCH_STDOUT_AND_STDERR
#define WIFI_CONNECT_COMMAND_BEGIN "sudo nmcli device wifi connect"

#define DELETE_UPLOAD_PROGRAMS_FILES "sudo rm -rf " VMS_PROGRAM_UPLOADER_DIR "programs/"

extern uint8_t evsp_prog[RTM_MAX];
extern pthread_mutex_t VMS_request_priority_mutex;

typedef struct VMS_update_args {
    uint8_t program_id;
    char *program_name;
} VMS_update_args;

void *vms_handler();
void vms_handler_init();
void vms_set_serial_attribs();

void vms_request_start(uint8_t id, uint8_t priority);
void vms_request_end(uint8_t id);
int carousel_update(uint8_t VMS_ID, uint8_t Program_Type, uint8_t Program_ID);
void VMS_report_programs_id(uint8_t cmd);
void VMS_report_program_name(uint8_t cmd, uint8_t program_id);
int VMS_search_program(char *program_name);
void *VMS_program_update(void *data);
bool vms_program_update_thread_activate(uint8_t Program_ID, char *Program_Name);

#endif