#ifndef VMS_H
#define VMS_H

#include <signal.h>
#include "time.h"
#include <pthread.h>
#include <stdint.h>

#define VMS_BAUDRATE B115200
#define VMS_SERIAL_PORT "/dev/ttyS0"
#define VMIN_LEN 20

#define RTM_MAX 8

#define VMS_PACKET_TX_LEN_MAX 40
#define VMS_PACKET_RX_LEN_MAX 100
#define VMS_PACKET_BEGIN "("
#define VMS_PACKET_COMMA ","
#define VMS_PACKET_END "\n"

#define CAROUSEL_NUM 255
#define VMS_ERROR_THRESHOLD 10

extern uint8_t evsp_prog[RTM_MAX];
extern pthread_mutex_t VMS_request_priority_mutex;

void *vms_handler();
void vms_handler_init();
void vms_set_serial_attribs();

void vms_request_start(uint8_t id, uint8_t priority);
void vms_request_end(uint8_t id);
int carousel_update(uint8_t VMS_ID, uint8_t Program_Type, uint8_t Program_ID);

#endif