#ifndef TYPEDEFINE_H
#define TYPEDEFINE_H

#include <stdint.h>
#include <stdbool.h>
#define __USE_XOPEN // TO SOLVE WARNING MSG: implicit declaration of function ‘strptime’
#include <time.h>

#define OBU_ID_MAX_LEN 10
#define RSU_ID_MAX_LEN 10
#define COMPENSATION_MAX_LEN 15
#define ID_MAX_LEN 15
#define APP_NAME_MAX_LEN 10
#define TIMESTAMP_LEN 19
#define OBU_RECORD_RING_CAPACITY 5 /* should be 3 ~ 256 */
#define STATIC_APP_PRIVATE_SPACE_CAPACITY 256
#define PHASE_COUNT_MAX_NUM 8
#define RESTART_TOKEN "e5WJjskIJNGn1anL"
#define TOKEN_LEN 16
typedef struct msg_obj msg_obj_t;

typedef enum device_type {
    DEVICE_OBU = 0,
    DEVICE_RSU = 1,
    DEVICE_CLOUD = 2,
    DEVICE_TYPE_NUMBER
} device_type_t;

typedef enum vehicle_type {
    VEHICLE_NORMAL = 0,
    VEHICLE_AMBULANCE = 1,
    VEHICLE_BUS = 2,
    VEHICLE_FIRE_TRUCK = 3,
    VEHICLE_POLICE_CAR = 4,
    VEHICLE_TYPE_NUMBER
} vehicle_type_t;

typedef enum traffic_signal_controller_manufacturer {
    CHENG_LONG = 0,
    SHAN_ZHU = 1,
    SHAN_ZHU_M = 2,
} traffic_signal_controller_manufacturer_t;

typedef enum event_type {
    EVENT_OBU_PACKET_RX = 0,
    EVENT_OBU_PACKET_TX = 1,
    EVENT_RSU_PACKET_RX = 2,
    EVENT_RSU_PACKET_TX = 3,
    EVENT_CLOUD_PACKET_RX = 4,
    EVENT_CLOUD_PACKET_TX = 5,
    EVENT_TRAFFIC_SIGNAL_COMMAND_TX = 6,
    EVENT_REGISTRATION = 7,
    EVENT_TYPE_NUMBER
} event_type_t;

typedef enum timer_event_type {
    TIMER_EVENT_TRAFFIC_SIGNAL_STATUS_REPORT = 0,
    TIMER_EVENT_TRAFFIC_SIGNAL_COMMAND_BUF_POLLING = 1,
    TIMER_EVENT_OBU_LIST_GARBAGE_COLLECTION = 2,
    TIMER_EVENT_LOG_FILE_NAME_UPDATE = 3,
    // TIMER_EVENT_DSRC_HEARTBIT_DETECT = 4,
    TIMER_EVENT_TYPE_NUMBER
} timer_event_type_t;

typedef struct config_object {
    char RSU_id[RSU_ID_MAX_LEN];
    float RSU_lat;
    float RSU_lon;
    uint8_t signal_controller_manufacturer;
    
    bool signal_status_report_active;
    bool signal_adjust_upper_bound_active;
    bool signal_adjust_lower_bound_active;
    float signal_adjust_upper_bound_percentage;
    float signal_adjust_lower_bound_percentage;
    uint8_t traffic_compensation_method;
    uint8_t traffic_compensation_cycle_number;
    float phase_weight[PHASE_COUNT_MAX_NUM];

    bool log_middleware_timer_event;
    bool log_application_register_event;
    bool log_command_buffer;    //what for??
    bool log_signal_packet_rx;
    bool log_signal_packet_tx;
    bool log_signal_packet_info;
    bool log_cloud_packet_rx;
    bool log_cloud_packet_tx;
    bool log_OBU_packet_rx;
    bool log_OBU_packet_tx;
    bool log_OBU_list;
} config_object_t;

typedef struct EVSP_config_object {
    uint8_t evsp_host_obu_packet_timeout;
    uint8_t evsp_host_obu_list_timeout;
    uint8_t min_green;
    uint8_t max_green;
    uint8_t valid_record_distance;
} EVSP_config_object_t;

typedef struct TSP_config_object {
    uint8_t tsp_host_obu_list_timeout;
    uint16_t tsp_remaining_distance_max;
} TSP_config_object_t;


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
    int (*on_traffic_signal_command_tx)(void *);
    int (*on_registration)(void *);
    struct application_object *next;
} app_obj_t;

typedef struct event_callback {
    char name[APP_NAME_MAX_LEN];
    uint8_t app_id;
    uint8_t priority;
    int (*callback)(void *);
    struct event_callback *next;
} event_callback_t;

typedef struct OBU_record {
    char OBU_id[OBU_ID_MAX_LEN];
    struct tm time_stamp;
    time_t time_second;
    float position_lon;
    float position_lat;
    uint8_t speed;
    uint8_t acceleration;
    uint8_t direction;
    uint8_t vehicle_type;
} OBU_record_t;

typedef struct OBU_record_ring {
    OBU_record_t record[OBU_RECORD_RING_CAPACITY];
    uint8_t first_record_pointer; // queue.front
    uint8_t last_record_pointer; // queue.back
    uint8_t length;
} OBU_record_ring_t;

typedef struct application_private_space {
    uint8_t static_space[STATIC_APP_PRIVATE_SPACE_CAPACITY];
    uint8_t *dynamic_space;
} app_private_space_t;


typedef struct OBU_object {
    char OBU_id[OBU_ID_MAX_LEN + 1];    //+1 if for \0
    uint8_t vehicle_type;
    OBU_record_ring_t record_ring;
    app_private_space_t *private_space;
    struct OBU_object *prev;
    struct OBU_object *next;
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
    uint8_t INFO[]; //這是指標？
} traffic_signal_packet_t;

typedef struct static_plan {
    // 5F C5
    uint16_t Green;
    // 5F C4
    uint8_t MinGreen;
    uint16_t MaxGreen;
    uint8_t Yellow;
    uint8_t AllRed;
    uint8_t PedGreenFlash;
    uint8_t PedRed;

    // Green - PedGreenFlash
    uint16_t PreGreen;  //原始步階1
    uint16_t PreTimeCompensated;
} static_plan_t;

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
    uint8_t SubPhaseCount; //1~8
    uint16_t CycleTime;
    uint16_t Offset;

    static_plan_t plan[PHASE_COUNT_MAX_NUM];

    uint8_t control_status;
} traffic_signal_status_t;

typedef struct msg_buf {
    unsigned char *content;
    uint32_t index;
} msg_buf_t;

typedef struct C2R_common_field {
    uint32_t packet_len;
    uint8_t device_type;
    char RSU_id[RSU_ID_MAX_LEN];
    char timestamp[TIMESTAMP_LEN];
    uint8_t service_id;
} C2R_common_field_t;

typedef struct R2C_common_field {
    uint32_t packet_len;
    char RSU_id[RSU_ID_MAX_LEN];
    char timestamp[TIMESTAMP_LEN];
    float position_lon;
    float position_lat; 
    uint8_t service_id;
} R2C_common_field_t;

typedef struct V2R_common_field {
    uint32_t packet_len;
    uint8_t device_type;
    char OBU_id[OBU_ID_MAX_LEN];
    uint8_t vehicle_type;
    char timestamp[TIMESTAMP_LEN];
    float position_lon;
    float position_lat;
    uint8_t speed;
    uint8_t direction;
    uint8_t service_id;
} V2R_common_field_t;

typedef struct C2R_app_section {
    uint32_t payload_len;
    char *payload;
    uint8_t com_id;
} C2R_app_section_t;

typedef struct V2R_app_section {
    uint32_t payload_len;
    char *payload;
    uint8_t com_id;
    OBU_object_t *OBU_object;
} V2R_app_section_t;

typedef struct tsc_command {
    uint8_t app_id;
    uint8_t app_priority;
    uint8_t target_phase;
    // cycle and phase are used to "indicate the index of the target_command buffer object".
    uint8_t cycle;
    uint8_t phase;
    int16_t effect_time;// is the length of time that the application requests to be adjusted to.
    int8_t adjustment;// the adjustment of time that the application requests to be adjusted.
    int8_t compensation_time;
    char host_OBU_id[ID_MAX_LEN + 1];
} tsc_command_t;

// Each element of the command buffer is a command buffer object.
typedef struct tsc_command_object { 
    uint8_t app_id;
    uint8_t app_priority;
    uint8_t target_phase;
    uint8_t effect_time;// is the length of time that the application requests to be adjusted to. 
    uint8_t adjusted_time;// is the length of time that the traffic signal controller is adjusted to.
    int8_t compensation_time;
    char host_OBU_id[ID_MAX_LEN + 1];
    bool send_flag;
} tsc_command_object_t;

typedef struct traffic_signal_command_arg {
    uint8_t control_status;
    uint8_t phase;
    uint8_t step;
    uint8_t effect_time;
    char host_OBU_id[OBU_ID_MAX_LEN + 1];
} traffic_signal_command_arg_t;

#endif