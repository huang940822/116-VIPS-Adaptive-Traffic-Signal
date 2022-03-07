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
uint8_t flag_pretime = 0;
uint8_t flag_countdown_on = 0;
uint8_t flag_countdown_off = 0;
uint8_t flag_query_firm_ver = 0;
uint8_t flag_switch2nextStep = 0;

uint8_t get_seq_num()
{
    pthread_mutex_lock(&mutex_seq_num);
    uint8_t value = seq_num++;
    pthread_mutex_unlock(&mutex_seq_num);
    return value;
}
//要求要動態
uint8_t tsc_dynamic()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: DYNAMIC\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_dynamic: malloc");
        perror("tsc_dynamic: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = DYNAMIC_LEN0_VAL;
    packet->LEN[1] = DYNAMIC_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x10;
    packet->INFO[2] = 0x15;
    //改成120 decimal不要是永遠
    packet->INFO[3] = 0x78;

    uint8_t output_byte[DYNAMIC_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];  // why -1?
    uint8_t info_byte[DYNAMIC_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, DYNAMIC_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, DYNAMIC_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < DYNAMIC_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[DYNAMIC_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[DYNAMIC_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < DYNAMIC_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, DYNAMIC_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != DYNAMIC_LEN1_VAL) {
        log_file_write_fatal_error("tsc_dynamic: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

uint8_t tsc_pretime()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: PRETIME\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_pretime: malloc");
        perror("tsc_pretime: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = PRETIME_LEN0_VAL;
    packet->LEN[1] = PRETIME_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x10;
    packet->INFO[2] = 0x05;
    // packet->INFO[2] = 0x01; //照介庸建議
    packet->INFO[3] = 0x00;

    uint8_t output_byte[PRETIME_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[PRETIME_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, PRETIME_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, PRETIME_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < PRETIME_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[PRETIME_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[PRETIME_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < PRETIME_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, PRETIME_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != PRETIME_LEN1_VAL) {
        log_file_write_fatal_error("tsc_pretime: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

//強制到下一個step?沒用到
uint8_t tsc_switch()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: SWITCH\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_switch: malloc");
        perror("tsc_switch: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = SWITCH_LEN0_VAL;
    packet->LEN[1] = SWITCH_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x1C;
    packet->INFO[2] = 0x00;
    packet->INFO[3] = 0x00;
    packet->INFO[4] = 0x00;

    uint8_t output_byte[SWITCH_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[SWITCH_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, SWITCH_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, SWITCH_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < SWITCH_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[SWITCH_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[SWITCH_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < SWITCH_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, SWITCH_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != SWITCH_LEN1_VAL) {
        log_file_write_fatal_error("tsc_switch: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
    ;
}

//注意成龍的部份 這裡是改變每個step的時間
uint8_t tsc_extend(uint8_t subphase, uint8_t step, uint8_t effect_time)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: EXTEND\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_extend: malloc");
        perror("tsc_extend: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

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

    uint8_t output_byte[EXTEND_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[EXTEND_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, EXTEND_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, EXTEND_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < EXTEND_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[EXTEND_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[EXTEND_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < EXTEND_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, EXTEND_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != EXTEND_LEN1_VAL) {
        log_file_write_fatal_error("tsc_extend: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}


//沒用到
// void tsc_EVSP_on(uint8_t subphase)
// {
//     char log_content[LOG_CONTENT_LEN + 1];
//     memset(log_content, 0, sizeof(log_content));
//     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
//     strlen(log_content), "signal packet tx: EVSP 0N\n");

//     traffic_signal_packet_t *packet = (traffic_signal_packet_t
//     *)malloc(MAX_PACKET_LEN); if (packet == NULL) {
//         set_memory_error();
// 		log_file_write_fatal_error("tsc_EVSP_on: malloc");
//         perror("tsc_EVSP_on: malloc");
//         exit(errno);
//     } else {
//         clear_memory_error();
//         memset(packet, 0, MAX_PACKET_LEN);
//     }
//     packet->DLE_1 = DLE_VAL;
//     packet->TYPE = STX_VAL;
//     packet->ADDR[0] = ADDR0_VAL;
//     packet->ADDR[1] = ADDR1_VAL;
//     packet->DLE_2 = DLE_VAL;
//     packet->ETX = ETX_VAL;

//     packet->SEQ = get_seq_num();
//     packet->LEN[0] = EVSP_LEN0_VAL;
//     packet->LEN[1] = EVSP_LEN1_VAL;
//     packet->INFO[0] = 0x5F;
//     packet->INFO[1] = 0x19;
//     packet->INFO[2] = subphase;
//     packet->INFO[3] = 0x01;
//     packet->INFO[4] = 0x00;
//     packet->INFO[5] = 0xFF;
//     packet->INFO[6] = 0xEA;

//     uint8_t output_byte[EVSP_LEN1_VAL];
//     uint8_t header_byte[HEADER_LEN - 1];
//     uint8_t info_byte[EVSP_LEN1_VAL - HEADER_LEN];
//     uint8_t CKS = 0;

//     CKS = check_sum(packet, EVSP_LEN1_VAL - HEADER_LEN);

//     memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
//     memcpy(info_byte, &packet->INFO, EVSP_LEN1_VAL - HEADER_LEN);

//     for (int i = 0; i < 7; i++) {
//         output_byte[i] = header_byte[i];
//     }
//     for (int i = 0; i < EVSP_LEN1_VAL - HEADER_LEN; i++) {
//         output_byte[i + 7] = info_byte[i];
//     }
//     for (int i = 0; i < 2; i++) {
//         output_byte[EVSP_LEN1_VAL - 3 + i] = header_byte[i + 7];
//     }
//     output_byte[EVSP_LEN1_VAL - 1] = CKS;

//     if (config.log_signal_packet_tx) {
//         for (int i = 0; i < EVSP_LEN1_VAL; i++) {
//             snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
//             strlen(log_content), "%x ", output_byte[i]);
//         }
//         log_file_write(log_content);
//     }

//     int ret = write(serial_port_fd, output_byte, EVSP_LEN1_VAL);
//     if (ret == -1 || ret != EVSP_LEN1_VAL) {
// 		log_file_write_fatal_error("tsc_EVSP_on: write");
//     }
//     // tcdrain(serial_port_fd);
//     if (packet != NULL) {
//         free(packet);
//     }
//     return;
// }
// //沒用到
// void tsc_EVSP_off()
// {
//     char log_content[LOG_CONTENT_LEN + 1];
//     memset(log_content, 0, sizeof(log_content));
//     snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
//     strlen(log_content), "signal packet tx: EVSP 0FF\n");

//     traffic_signal_packet_t *packet = (traffic_signal_packet_t
//     *)malloc(MAX_PACKET_LEN); if (packet == NULL) {
//         set_memory_error();
// 		log_file_write_fatal_error("tsc_EVSP_off: malloc");
//         perror("tsc_EVSP_off: malloc");
//         exit(errno);
//     } else {
//         clear_memory_error();
//         memset(packet, 0, MAX_PACKET_LEN);
//     }
//     packet->DLE_1 = DLE_VAL;
//     packet->TYPE = STX_VAL;
//     packet->ADDR[0] = ADDR0_VAL;
//     packet->ADDR[1] = ADDR1_VAL;
//     packet->DLE_2 = DLE_VAL;
//     packet->ETX = ETX_VAL;

//     packet->SEQ = get_seq_num();
//     packet->LEN[0] = EVSP_LEN0_VAL;
//     packet->LEN[1] = EVSP_LEN1_VAL;
//     packet->INFO[0] = 0x5F;
//     packet->INFO[1] = 0x19;
//     packet->INFO[2] = 0x00;
//     packet->INFO[3] = 0x01;
//     packet->INFO[4] = 0x00;
//     packet->INFO[5] = 0xFF;
//     packet->INFO[6] = 0xEF;

//     uint8_t output_byte[EVSP_LEN1_VAL];
//     uint8_t header_byte[HEADER_LEN - 1];
//     uint8_t info_byte[EVSP_LEN1_VAL - HEADER_LEN];
//     uint8_t CKS = 0;

//     CKS = check_sum(packet, EVSP_LEN1_VAL - HEADER_LEN);

//     memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
//     memcpy(info_byte, &packet->INFO, EVSP_LEN1_VAL - HEADER_LEN);

//     for (int i = 0; i < 7; i++) {
//         output_byte[i] = header_byte[i];
//     }
//     for (int i = 0; i < EVSP_LEN1_VAL - HEADER_LEN; i++) {
//         output_byte[i + 7] = info_byte[i];
//     }
//     for (int i = 0; i < 2; i++) {
//         output_byte[EVSP_LEN1_VAL - 3 + i] = header_byte[i + 7];
//     }
//     output_byte[EVSP_LEN1_VAL - 1] = CKS;

//     if (config.log_signal_packet_tx) {
//         for (int i = 0; i < EVSP_LEN1_VAL; i++) {
//             snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN -
//             strlen(log_content), "%x ", output_byte[i]);
//         }
//         log_file_write(log_content);
//     }

//     int ret = write(serial_port_fd, output_byte, EVSP_LEN1_VAL);
//     if (ret == -1 || ret != EVSP_LEN1_VAL) {
// 		log_file_write_fatal_error("tsc_EVSP_off: write");
//     }
//     // tcdrain(serial_port_fd);
//     if (packet != NULL) {
//         free(packet);
//     }
//     return;
// }
// query SubPhaseID StepID
uint8_t tsc_5F4C()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: 5F4C\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_5F4C: malloc");
        perror("tsc_5F4C: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x4C;

    uint8_t output_byte[QUERY_LEN1_VAL + 1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[QUERY_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, QUERY_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, QUERY_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[QUERY_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[QUERY_LEN1_VAL - 1] = CKS;
    output_byte[QUERY_LEN1_VAL] = '\0';

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    // printf("get in 5f4c lock\r\n");
    int ret = write(serial_port_fd, output_byte, QUERY_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);
    // printf("release 5f4c lock\r\n");

    if (ret == -1 || ret != QUERY_LEN1_VAL) {
        log_file_write_fatal_error("tsc_5F4C: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}
// query PlanID
uint8_t tsc_5F48()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: 5F48\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_5F48: malloc");
        perror("tsc_5F48: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x48;

    uint8_t output_byte[QUERY_LEN1_VAL + 1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[QUERY_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, QUERY_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, QUERY_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[QUERY_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[QUERY_LEN1_VAL - 1] = CKS;
    output_byte[QUERY_LEN1_VAL] = '\0';

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    // printf("get in 5f48 lock\r\n");
    int ret = write(serial_port_fd, output_byte, QUERY_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);
    // printf("release 5f48 lock\r\n");

    if (ret == -1 || ret != QUERY_LEN1_VAL) {
        log_file_write_fatal_error("tsc_5F48: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}
// query Plan Info
uint8_t tsc_5F44()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: 5F44\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_5F44: malloc");
        perror("tsc_5F44: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x44;
    packet->INFO[2] = get_plan_id();

    uint8_t output_byte[QUERY_PLAN_LEN1_VAL + 1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[QUERY_PLAN_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, QUERY_PLAN_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, QUERY_PLAN_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_PLAN_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[QUERY_PLAN_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[QUERY_PLAN_LEN1_VAL - 1] = CKS;
    output_byte[QUERY_PLAN_LEN1_VAL] = '\0';

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_PLAN_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    // printf("get in 5f44 lock\r\n");
    int ret = write(serial_port_fd, output_byte, QUERY_PLAN_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);
    // printf("release 5f44 lock\r\n");
    if (ret == -1 || ret != QUERY_PLAN_LEN1_VAL) {
        log_file_write_fatal_error("tsc_5F44: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}
// query Plan Info (Green)
uint8_t tsc_5F45()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: 5F45\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_5F45: malloc");
        perror("tsc_5F45: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_PLAN_LEN0_VAL;
    packet->LEN[1] = QUERY_PLAN_LEN1_VAL;
    packet->INFO[0] = 0x5F;
    packet->INFO[1] = 0x45;
    packet->INFO[2] = get_plan_id();

    uint8_t output_byte[QUERY_PLAN_LEN1_VAL + 1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[QUERY_PLAN_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, QUERY_PLAN_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, QUERY_PLAN_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_PLAN_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[QUERY_PLAN_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[QUERY_PLAN_LEN1_VAL - 1] = CKS;
    output_byte[QUERY_PLAN_LEN1_VAL] = '\0';

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_PLAN_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    // printf("get in 5f45 lock\r\n");
    int ret = write(serial_port_fd, output_byte, QUERY_PLAN_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);
    // printf("release 5f45 lock\r\n");

    if (ret == -1 || ret != QUERY_PLAN_LEN1_VAL) {
        log_file_write_fatal_error("tsc_5F45: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

// query date and time
uint8_t tsc_0F42()
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: 0F42\n");

    traffic_signal_packet_t *packet = (traffic_signal_packet_t *)malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
		log_file_write_fatal_error("tsc_0F42: malloc");
        perror("tsc_0F42: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

    packet->SEQ = get_seq_num();
    packet->LEN[0] = QUERY_LEN0_VAL;
    packet->LEN[1] = QUERY_LEN1_VAL;
    packet->INFO[0] = 0x0F;
    packet->INFO[1] = 0x42;

    uint8_t output_byte[QUERY_LEN1_VAL+1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[QUERY_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, QUERY_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, QUERY_LEN1_VAL - HEADER_LEN);
    
    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[QUERY_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[QUERY_LEN1_VAL - 1] = CKS;
    output_byte[QUERY_LEN1_VAL] = '\0';

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content), LOG_CONTENT_LEN - strlen(log_content), "%x ", output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    // printf("get in 5f48 lock\r\n");
    int ret = write(serial_port_fd, output_byte, QUERY_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);
    // printf("release 5f48 lock\r\n");

    if (ret == -1 || ret != QUERY_LEN1_VAL) {
		log_file_write_fatal_error("tsc_0F42: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

// countdown on
uint8_t tsc_countdown_on(uint8_t machine_type)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: COUNTDOWN ON\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_countdown_on: malloc");
        perror("tsc_countdown_on: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

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
        packet->INFO[6] = 0xFE;  //晟隆
    } else if (machine_type == 1) {
        packet->INFO[6] = 0xFF;  //山竚
    } else {
        printf("unknown tc machine\r\n");
    }


    uint8_t output_byte[EVSP_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[EVSP_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, EVSP_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, EVSP_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < EVSP_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[EVSP_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[EVSP_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < EVSP_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, EVSP_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != EVSP_LEN1_VAL) {
        log_file_write_fatal_error("tsc_countdown_on: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

// countdown off
uint8_t tsc_countdown_off(uint8_t machine_type)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: COUNTDOWN OFF\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("tsc_countdown_off: malloc");
        perror("tsc_countdown_off: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

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
        packet->INFO[6] = 0xFF;  //晟隆
    } else if (machine_type == 1) {
        packet->INFO[6] = 0xFE;  //山竚
    } else {
        printf("unknown tc machine\r\n");
    }


    uint8_t output_byte[EVSP_LEN1_VAL];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[EVSP_LEN1_VAL - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, EVSP_LEN1_VAL - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, EVSP_LEN1_VAL - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < EVSP_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[EVSP_LEN1_VAL - 3 + i] = header_byte[i + 7];
    }
    output_byte[EVSP_LEN1_VAL - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < EVSP_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, EVSP_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != EVSP_LEN1_VAL) {
        log_file_write_fatal_error("tsc_countdown_off: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}

// query version of tsc
uint8_t tsc_query_firmware_version(void)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content),
             "signal packet tx: tsc version query\n");

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("query firmware version: malloc");
        perror("query firmware version: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    packet->DLE_1 = DLE_VAL;
    packet->TYPE = STX_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->DLE_2 = DLE_VAL;
    packet->ETX = ETX_VAL;

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


    uint8_t output_byte[FIRMQ_LEN1];
    uint8_t header_byte[HEADER_LEN - 1];
    uint8_t info_byte[FIRMQ_LEN1 - HEADER_LEN];
    uint8_t CKS = 0;

    CKS = check_sum(packet, FIRMQ_LEN1 - HEADER_LEN);

    memcpy(header_byte, &packet->DLE_1, HEADER_LEN - 1);
    memcpy(info_byte, &packet->INFO, FIRMQ_LEN1 - HEADER_LEN);

    for (int i = 0; i < 7; i++) {
        output_byte[i] = header_byte[i];
    }
    for (int i = 0; i < QUERY_PLAN_LEN1_VAL - HEADER_LEN; i++) {
        output_byte[i + 7] = info_byte[i];
    }
    for (int i = 0; i < 2; i++) {
        output_byte[FIRMQ_LEN1 - 3 + i] = header_byte[i + 7];
    }
    output_byte[FIRMQ_LEN1 - 1] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < QUERY_PLAN_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }
    pthread_mutex_lock(&mutex_rs232_write);
    int ret = write(serial_port_fd, output_byte, QUERY_PLAN_LEN1_VAL);
    pthread_mutex_unlock(&mutex_rs232_write);

    if (ret == -1 || ret != QUERY_PLAN_LEN1_VAL) {
        log_file_write_fatal_error("tsc_version_query: write");
    }
    // tcdrain(serial_port_fd);
    if (packet != NULL) {
        free(packet);
    }
    return packet->SEQ;
}