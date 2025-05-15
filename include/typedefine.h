#ifndef TYPEDEFINE_H
#define TYPEDEFINE_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#define __USE_XOPEN  // TO SOLVE WARNING MSG: implicit declaration of function ‘strptime’
#include <time.h>
#include "j2735_map.h"
#include "j2735_msg.h"
#include "util.h"

#define FILE_PATH "./"
#define OBU_NAME_MAX_LEN 10
#define RSU_NAME_MAX_LEN 10
#define ID_MAX_LEN 15
#define APP_NAME_MAX_LEN 10
#define TIMESTAMP_LEN 19
#define OBU_RECORD_RING_CAPACITY 5 /* should be 3 ~ 256 */
#define STATIC_APP_PRIVATE_SPACE_CAPACITY 256
#define PHASE_COUNT_MAX_NUM 8
#define SIGNAL_COUNT_MAX_NUM 8  // 岔路數目
#define PLANID_MAX_NUM 48
#define RESTART_TOKEN "e5WJjskIJNGn1anL"
#define TOKEN_LEN 16
#define PROGRAM_NAME_LEN 100
#define WIFI_ADAPTER_DEVICE_NAME 50

// This define CPS_DEBUG is for CPS testing.
// It's for the log buffer size.
// If testing can set 1000 or other you want the positive number.
// Set negative for normal.
#define CPS_DEBUG -1

typedef struct msg_obj msg_obj_t;

typedef enum device_type {
    DEVICE_OBU = 0,
    DEVICE_RSU = 1,
    DEVICE_CLOUD = 2,
    DEVICE_TYPE_NUMBER
} device_type_t;

typedef enum application_id {
    MMP_ID = 0,
    EVSP_ID = 1,
    TSP_ID = 2,
    ATSC_ID = 3,
    CPS_ID = 4,
    SPAT_ID = 5,
    TIB_ID = 6,
    SPM_ID = 7,
    WA_ID = 8,
    APPLICATION_ID_NUMBER
} application_id_t;

typedef enum vehicle_type {
    VEHICLE_NORMAL = -1,
    VEHICLE_AMBULANCE = 0,
    VEHICLE_BUS = 1,
    VEHICLE_FIRE_TRUCK = 2,
    VEHICLE_POLICE_CAR = 3,
    VEHICLE_TYPE_NUMBER
} vehicle_type_t;

typedef enum event_type {
    EVENT_OBU_PACKET_RX = 0,
    EVENT_OBU_PACKET_TX = 1,
    EVENT_RSU_PACKET_RX = 2,
    EVENT_RSU_PACKET_TX = 3,
    EVENT_CLOUD_PACKET_RX = 4,
    EVENT_CLOUD_PACKET_TX = 5,
    EVENT_TRAFFIC_SIGNAL_COMMAND_TX = 6,
    EVENT_CAMERA_PACKET_RX = 7,
    EVENT_REGISTRATION = 8,
    EVENT_MIDDLEWARE_RESTART, /* enum will auto increase */
    EVENT_TYPE_NUMBER
} event_type_t;

typedef enum timer_event_type {
    TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT = 0,
    TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING = 1,
    TIMER_EVENT_LOG_FILE_NAME_UPDATE = 2,
    TIMER_EVENT_DSRC_SEND = 3,
    // TIMER_EVENT_DSRC_HEARTBIT_DETECT = 4,
    TIMER_EVENT_TYPE_NUMBER
} timer_event_type_t;

typedef enum position {
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    NORTHWEST,
} position_t;

typedef enum signalstatus {
    RED = 1,
    YELLOW = 2,
    GREEN = 4,  // 圓頭綠
    LEFT_GREEN = 8,
    STRAIGHT_GREEN = 16,
    RIGHT_GREEN = 32,
    PEDESTRIAN_GREEN = 64,
    PEDESTRIAN_RED = 128,
} SignalStatus_t;

typedef struct external_app_info_type {
    pid_t pid;             // process id of the external application
    int notify_fd;         // middleware use this socket_fd to notify the app
    int interact_fd;       // app will use this socket_fd to call middleware-api
    uint8_t heartbeat_rc;  // heartbeat record, updated each time app send req to MW
} ea_info_t;

typedef struct application_object {
    char name[APP_NAME_MAX_LEN];
    uint8_t dontSend2TC;
    uint8_t id;
    uint8_t priority;
    uint8_t cloud_com_id;
    int (*on_OBU_packet_rx)(void *);
    int (*on_OBU_packet_tx)(void *);
    int (*on_RSU_packet_rx)(void *);
    int (*on_RSU_packet_tx)(void *);
    int (*on_cloud_packet_rx)(void *);
    int (*on_cloud_packet_tx)(void *);
    int (*on_camera_packet_rx)(void *);
    int (*on_traffic_signal_command_tx)(void *);
    int (*on_registration)(void *);
    int (*on_middleware_restart)(void *);
    ea_info_t *ea_info_p;  // if this is not NULL, indicate this is an external app
    struct application_object *next;
} app_obj_t;

typedef enum {
    event_callback_id_app_id,
    event_callback_id_msg_id,
} event_callback_id_choice;

typedef struct event_callback_id {
    event_callback_id_choice choice;
    union {
        int app_id;
        DSRCmsgID msg_id;
    } u;

} event_callback_id_t;

typedef struct event_callback {
    char name[APP_NAME_MAX_LEN];
    event_callback_id_t event_callback_id;
    uint8_t priority;
    int (*callback)(void *);
    struct event_callback *next;
    app_obj_t *app_obj_p;
} event_callback_t;

typedef struct OBU_record_common_field {
    time_t time_second;
    time_t time_nsec;
    float position_lon;
    float position_lat;
    uint8_t speed;  // m/s
    uint8_t direction;
    char OBU_name[OBU_NAME_MAX_LEN + 1];
    vehicle_type_t vehicle_type;
    int fd;
} OBU_record_common_field_t;

typedef struct OBU_record {
    time_t time_second;
    time_t time_nsec;
    float position_lon;
    float position_lat;
    uint8_t speed;  // m/s
    uint8_t direction;
} OBU_record_t;

typedef struct OBU_record_ring {
    OBU_record_t record[OBU_RECORD_RING_CAPACITY];
    uint8_t first_record_pointer;  // queue.front
    uint8_t last_record_pointer;   // queue.back
    uint8_t length;
} OBU_record_ring_t;

typedef struct application_private_space {
    uint8_t static_space[STATIC_APP_PRIVATE_SPACE_CAPACITY];
    // uint8_t *dynamic_space;   //not in use currently
} app_private_space_t;

typedef enum {
    OBU_object_unknown = -1,
    OBU_object_processing = 0,
    OBU_object_granted = 1,
    OBU_object_rejected = 2,
} OBU_object_status;

typedef struct OBU_object {
    char OBU_name[ID_MAX_LEN + 1];  //+1 if for \0
    vehicle_type_t vehicle_type;    // enum type
    OBU_object_status status;       // enum type
    float prediction_speed;         // m/s
    OBU_record_ring_t record_ring;
    app_private_space_t *private_space;
    struct OBU_object *prev;  // should not pass to external app
    struct OBU_object *next;  // should not pass to external app
} OBU_object_t;

typedef struct traffic_signal_packet {
    uint8_t CKS;
    uint8_t DLE_1; /*Start of Packet*/
    uint8_t TYPE;  /*Type of Packet*/
    uint8_t SEQ;
    uint8_t ADDR[2];
    uint8_t LEN[2];
    uint8_t DLE_2;
    uint8_t ETX;
    uint8_t INFO[];  // 因為 info 長度是會變動並不是一個固定值
} traffic_signal_packet_t;

typedef struct static_plan {
    // 不包含步階 1 ，從步階 2 到步階 5
    union {
        uint8_t StepArr[4];
        struct
        {
            uint8_t PedGreenFlash;
            uint8_t PedRed;
            uint8_t Yellow;
            uint8_t AllRed;
        };
    };

    // Green - PedGreenFlash
    uint16_t PreGreen;  // 原始步階1
    // 正在執行的綠燈秒數
    // 會因為執行延長、縮短或補償動態改變
    // 如果沒有被延長或縮短等於 Green
    uint16_t PreTimeCompensated;

    // 5F C5
    uint16_t Green;
    // 5F C4
    uint16_t MaxGreen;
    uint8_t MinGreen;
} static_plan_t;

// 是一個 bitString 要對照 enum SignalStatus_t 來看做 flag
typedef struct phaseorder_plan {
    uint8_t SignalStatus;
} phaseorder_plan_t;

typedef struct AllDay_plan {
    uint8_t Hour;
    uint8_t Min;
    uint8_t PlanID;
} AllDay_plan_t;

typedef struct traffic_signal_status {
    // 5F CC
    uint8_t ControlStrategy;
    uint8_t SubPhaseID;
    uint8_t StepID;
    uint16_t StepSec;
    // 5F C8
    uint8_t PlanID;
    // 5F C5
    uint8_t PhaseOrder;
    uint8_t SubPhaseCount;  // 1~8
    uint16_t CycleTime;
    uint16_t Offset;

    // 0F C2
    uint8_t Year;   // (00~255) (國曆)
    uint8_t Month;  // (01~12)
    uint8_t Day;    // (01~31)
    uint8_t Week;   // (01~07)
    uint8_t Hour;   // (00~23)
    uint8_t Min;    // (00~59)
    uint8_t Sec;    // (00~59)
    // TC 與 IPC 相差的秒數 只有比較當天的相差 超過一天不會計算
    // 正數表示 TC 時間較快 負數表示 IPC 時間較快
    int tcTimeOffest;
    // 0F 04
    uint16_t original_tc_health_status;

    static_plan_t plan[PHASE_COUNT_MAX_NUM];

    uint8_t SignalMap;
    uint8_t SignalCount;
    phaseorder_plan_t phaseorder_plan[PHASE_COUNT_MAX_NUM][SIGNAL_COUNT_MAX_NUM];

    uint8_t control_status;

    // 5F C6
    uint8_t SegmentType;
    uint8_t SegmentCount;
    AllDay_plan_t allday_plan[PLANID_MAX_NUM];
} traffic_signal_status_t;

typedef struct msg_buf {
    unsigned char *content;
    uint32_t index;
} msg_buf_t;

typedef struct C2R_common_field {
    uint32_t packet_len;
    uint8_t device_type;
    char RSU_name[RSU_NAME_MAX_LEN];
    char timestamp[TIMESTAMP_LEN];
    uint8_t service_id;
} C2R_common_field_t;

typedef struct R2C_common_field {
    uint32_t packet_len;
    char RSU_name[RSU_NAME_MAX_LEN];
    char timestamp[TIMESTAMP_LEN];
    float position_lon;
    float position_lat;
    uint8_t service_id;
} R2C_common_field_t;

typedef struct V2R_common_field {
    uint32_t packet_len;
    uint8_t device_type;
    char OBU_name[OBU_NAME_MAX_LEN];
    uint8_t vehicle_type;
    struct tm timestamp;
    float position_lon;
    float position_lat;
    uint8_t speed;
    uint8_t direction;
    uint8_t service_id;
    char *payload;
    uint32_t payload_len;
} V2R_common_field_t;

typedef struct C2R_app_section {
    uint32_t payload_len;
    char *payload;
    // uint8_t com_id;
} C2R_app_section_t;

typedef struct V2R_app_section {
    uint32_t payload_len;
    char *payload;
    // uint8_t com_id;
    OBU_object_t *OBU_object;
    DSRCmsgID msgID;
    void *data;  // use j2735 lib to encode and decodes
} V2R_app_section_t; //汽車到遠端裝置app

typedef struct V2R_self_defined_section {
    char obu_name[OBU_NAME_MAX_LEN];
    uint8_t vehical_type;
    // char time_stamp[TIMESTAMP_LEN];
    time_t time_second;
    float lon;
    float lat;
    uint8_t speed;
    uint8_t direction;
    DSRCmsgID msgID; /* id of bsm or srm */
    size_t data_len; /* len of srm_msg or bsm_msg */
    void *data;      /* srm_msg or bsm_msg */
    union {
        struct {  // EVSP
            uint8_t evsp_on_duty_flag;
            uint8_t evsp_weight;
            uint8_t evsp_error_code;
        };

        struct {  // TSP
            uint8_t tsp_on_duty_flag;
            uint8_t tsp_passenger_num;
        };

        // struct{ }; //for future new app
    };
} V2R_self_defined_section_t;

typedef struct tsc_command {
    uint8_t app_id;
    uint8_t app_priority;
    uint8_t target_phase;
    // cycle and phase are used to "indicate the index of the target_command
    // buffer object".
    uint8_t cycle;
    uint8_t phase;
    int16_t effect_time;  // is the length of time that the application requests
                          // to be adjusted to.
    int8_t adjustment;    // the adjustment of time that the application requests
                          // to be adjusted.
    char host_OBU_name[ID_MAX_LEN + 1];
    vehicle_type_t vehicle_type;
} tsc_command_t;

// Each element of the command buffer is a command buffer object.
typedef struct tsc_command_object {
    uint8_t app_id;
    uint8_t app_priority;
    uint8_t target_phase;
    uint8_t effect_time;    // is the length of time that the application requests
                            // to be adjusted to.
    uint8_t adjusted_time;  // is the length of time that the traffic signal
                            // controller is adjusted to.
    char host_OBU_name[ID_MAX_LEN + 1];
    vehicle_type_t vehicle_type;
    bool send_flag;
} tsc_command_object_t;

typedef struct traffic_signal_command_arg {
    uint8_t control_status;
    uint8_t phase;
    uint8_t step;
    uint8_t effect_time;
    char host_OBU_name[ID_MAX_LEN + 1];
} traffic_signal_command_arg_t;

typedef struct wifi_adapter_device {
    char device_name[WIFI_ADAPTER_DEVICE_NAME + 1];
} wifi_adapter_device_t;

#endif