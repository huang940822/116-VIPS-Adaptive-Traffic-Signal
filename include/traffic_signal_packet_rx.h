#ifndef TRAFFIC_SIGNAL_PACKET_RX_H
#define TRAFFIC_SIGNAL_PACKET_RX_H

#include "time.h"
#include "typedefine.h"

#define TC_BAUDRATE B9600
#define TC_SERIAL_PORT "/dev/ttyS0"
#define DLE_VAL 0xAA
#define STX_VAL 0xBB
#define ETX_VAL 0xCC
#define ACK_VAL 0xDD
#define NAK_VAL 0xEE
#define ADDR0_VAL 0xFF
#define ADDR1_VAL 0xFF
#define ACK_LEN0_VAL 0x00
#define ACK_LEN1_VAL 0x08
#define NAK_LEN0_VAL 0x00
#define NAK_LEN1_VAL 0x09

#define ERR_CKS 0x1
#define ERR_FRAME 0x2
#define ERR_ADDR 0x4
#define ERR_LENGTH 0x8

#define HEADER_LEN 10       // packet_len - info_len
#define MAX_PACKET_LEN 128  // two bytes
#define MAX_PAYLOAD_LEN (MAX_PACKET_LEN - HEADER_LEN)
#define ACK_INFO_LEN 0
#define NAK_INFO_LEN 1
#define VMIN_LEN 20
#define ACK_TIMEOUTSEC 0.5
#define WAIT_ACK_LOOP                                               \
    do {                                                            \
        clock_t _startTime = clock();                               \
        clock_t _endTime = clock();                                 \
        while (temp_ack_seq != ack_seq) {                           \
            if (((double) _endTime - _startTime) / CLOCKS_PER_SEC > \
                ACK_TIMEOUTSEC)                                     \
                break;                                              \
            _endTime = clock();                                     \
        }                                                           \
    } while (0);


extern int serial_port_fd;
extern int16_t ack_seq;

void traffic_signal_port_init();
void *traffic_signal_packet_rx_handler();
int check_sum(traffic_signal_packet_t *, int);
void set_serial_attribs(int fd, int speed, char serial_port[]);

#endif
