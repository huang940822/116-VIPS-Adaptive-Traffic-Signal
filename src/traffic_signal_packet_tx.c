#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>  //realloc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memcpy
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_packet_tx.h"
#include "traffic_signal_status_updating.h"

uint8_t seq_num = 0;
pthread_mutex_t mutex_seq_num = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_rs232_write = PTHREAD_MUTEX_INITIALIZER;
uint8_t flag_countdown_on = 0;
uint8_t flag_countdown_off = 0;
uint8_t flag_query_firm_ver = 0;
uint8_t flag_switch2nextStep = 0;

#define signal_packet_init       \
    packet->DLE_1 = DLE_VAL;     \
    packet->TYPE = STX_VAL;      \
    packet->ADDR[0] = ADDR0_VAL; \
    packet->ADDR[1] = ADDR1_VAL; \
    packet->DLE_2 = DLE_VAL;     \
    packet->ETX = ETX_VAL;

uint8_t get_seq_num()
{
    pthread_mutex_lock(&mutex_seq_num);
    uint8_t value = seq_num++;
    pthread_mutex_unlock(&mutex_seq_num);
    return value;
}
// 要求要動態
uint8_t tsc_dynamic()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_dynamic");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = DYNAMIC_LEN0_VAL;
    packet->LEN[1] = DYNAMIC_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x10;
    packet->INFO[2] = 0x15;
    // 改成120 decimal不要是永遠
    packet->INFO[3] = 0x78;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: DYNAMIC");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

uint8_t tsc_pretime()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_pretime");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = PRETIME_LEN0_VAL;
    packet->LEN[1] = PRETIME_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x10;
    packet->INFO[2] = 0x05;
    // packet->INFO[2] = 0x01; //照介庸建議
    packet->INFO[3] = 0x00;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: PRETIME");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// 強制到下一個step?沒用到
uint8_t tsc_switch()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_switch");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = SWITCH_LEN0_VAL;
    packet->LEN[1] = SWITCH_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x1C;
    packet->INFO[2] = 0x00;
    packet->INFO[3] = 0x00;
    packet->INFO[4] = 0x00;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: SWITCH");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// 注意成龍的部份 這裡是改變每個step的時間
uint8_t tsc_extend(uint8_t subphase, uint8_t step, uint8_t effect_time)
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_extend");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = EXTEND_LEN0_VAL;
    packet->LEN[1] = EXTEND_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x1C;
    packet->INFO[2] = subphase;
    packet->INFO[3] = step;
    // printf("eff: %d\n", effect_time);
    // for escape character bug
    if (effect_time == 170) {
        effect_time = 171;
    }
    packet->INFO[4] = effect_time;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: EXTEND");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// 沒用到
void tsc_EVSP_on(uint8_t subphase)
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_EVSP_on");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = EVSP_LEN0_VAL;
    packet->LEN[1] = EVSP_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x19;
    packet->INFO[2] = subphase;
    packet->INFO[3] = 0x01;
    packet->INFO[4] = 0x00;
    packet->INFO[5] = 0xFF;
    packet->INFO[6] = 0xEA;

    TC_packet_tx(packet, "signal packet tx: EVSP 0N");
    if (packet != NULL) {
        free(packet);
    }
}

// 沒用到
void tsc_EVSP_off()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_EVSP_off");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = EVSP_LEN0_VAL;
    packet->LEN[1] = EVSP_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x19;
    packet->INFO[2] = 0x00;
    packet->INFO[3] = 0x01;
    packet->INFO[4] = 0x00;
    packet->INFO[5] = 0xFF;
    packet->INFO[6] = 0xEF;

    TC_packet_tx(packet, "signal packet tx: EVSP 0FF");
    if (packet != NULL) {
        free(packet);
    }
}

// query SubPhaseID StepID
uint8_t tsc_5F4C()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F4C");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x4C;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F4C");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query PlanID
uint8_t tsc_5F48()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F48");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x48;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F48");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query Plan Info
uint8_t tsc_5F44()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F44");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x44;
    packet->INFO[2] = get_plan_id();

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F44");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query Plan Info (Green)
uint8_t tsc_5F45()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F45");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x45;
    packet->INFO[2] = get_plan_id();

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F45");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// 查詢號誌控制器之時向排列, response 5FC3
uint8_t tsc_5F43()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F43");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x43;
    packet->INFO[2] = get_PhaseOrder();

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F43");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query date and time, response 0FC2
uint8_t tsc_0F42()
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_0F42");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x0F;
    packet->INFO[1] = 0x42;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 0F42");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query plan of all day, response 5FC6
uint8_t tsc_5F46(uint8_t WeekDay) 
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_5F46");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_SEGMENT_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_SEGMENT_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x46;
    packet->INFO[2] = 0xFF;
    packet->INFO[3] = WeekDay;

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: 5F46");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// countdown on
uint8_t tsc_countdown_on(uint8_t machine_type)
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_countdown_on");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = EVSP_LEN0_VAL;
    packet->LEN[1] = EVSP_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x19;
    packet->INFO[2] = 0x00;
    packet->INFO[3] = 0x01;
    packet->INFO[4] = 0xFF;
    packet->INFO[5] = 0xFF;
    // for the reversed setup of 晟隆 and 山竚
    if (machine_type == 0 || machine_type == 2) {
        packet->INFO[6] = 0xFE;  // 晟隆
    } else if (machine_type == 1) {
        packet->INFO[6] = 0xFF;  // 山竚
    } else {
        printf("unknown tc machine\r\n");
    }

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: COUNTDOWN ON");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// countdown off
uint8_t tsc_countdown_off(uint8_t machine_type)
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "tsc_countdown_off");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = EVSP_LEN0_VAL;
    packet->LEN[1] = EVSP_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x19;
    packet->INFO[2] = 0x00;
    packet->INFO[3] = 0x01;
    packet->INFO[4] = 0xFF;
    packet->INFO[5] = 0xFF;
    // for the reversed setup of 晟隆 and 山竚
    if (machine_type == 0 || machine_type == 2) {
        packet->INFO[6] = 0xFF;  // 晟隆
    } else if (machine_type == 1) {
        packet->INFO[6] = 0xFE;  // 山竚
    } else {
        printf("unknown tc machine\r\n");
    }

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: COUNTDOWN OFF");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

// query version of tsc 0F43
uint8_t tsc_query_firmware_version(void)
{
    traffic_signal_packet_t *packet;
    Malloc(packet, MAX_PACKET_LEN, "query firmware version");

    signal_packet_init;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = FIRMQ_LEN0;
    packet->LEN[1] = FIRMQ_LEN1;
    packet->INFO[0] = 0x0F;
    packet->INFO[1] = 0x43;
    // packet->INFO[2] = 0x00;
    // packet->INFO[3] = 0x01;
    // packet->INFO[4] = 0xFF;
    // packet->INFO[5] = 0xFF;
    // for the reversed setup of 晟隆 and 山竚
    // if(machine_type==0){
    // packet->INFO[6] = 0xFF; //晟隆
    // }else if(machine_type==1){
    // packet->INFO[6] = 0xFE; //山竚
    // }else{
    // printf("unknown tc machine\r\n");
    // }

    uint8_t ret = TC_packet_tx(packet, "signal packet tx: tsc version query");
    if (packet != NULL) {
        free(packet);
    }
    return ret;
}

uint8_t TC_packet_tx(traffic_signal_packet_t *packet, char *describe)
{
    uint16_t packet_len = be16toh(*(uint16_t *) packet->LEN);
    uint8_t output_byte[packet_len];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[packet_len - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, packet_len - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, packet_len - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < packet_len - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[packet_len - 3 + i] = header_byte[i + 7];
    }
    output_byte[packet_len - 1] = CKS;

    if (config.log_signal_packet_tx) {
        char log_content[LOG_CONTENT_LEN + 1] = {0};
        snprintf(log_content, sizeof(log_content), "%s\n", describe);
        for (int i = 0; i < packet_len; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }

    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, packet_len);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != packet_len) {
        log_file_write_fatal_error("%s: write", describe);
    }
    return packet->SEQ;
}