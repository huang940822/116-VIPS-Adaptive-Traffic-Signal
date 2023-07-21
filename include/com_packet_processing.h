#ifndef COM_PACKET_PROCESSING_H
#define COM_PACKET_PROCESSING_H

#include "buffer.h"
#include "typedefine.h"

#define R2C_COMMON_FIELD_LEN 43
#define R2V_COMMON_FIELD_LEN 43
#define C2R_COMMON_FIELD_LEN 35
#define V2R_COMMON_FIELD_LEN 46U
#define V2R_BSM_REGIONAL_LEN 2

#define R2C_SPECIFIC_FIELD_MAX_LEN 1000
#define R2V_SPECIFIC_FIELD_MAX_LEN 1000

void cloud_packet_tx(uint16_t len,
                     uint8_t service_id,
                     unsigned char *specific_field);
void OBU_packet_tx(uint16_t len,
                   uint8_t service_id,
                   unsigned char *specific_field);
int cloud_packet_rx_event_handler(msg_obj_t *msg);
int OBU_packet_rx_event_handler(msg_obj_t *msg);
double Smart_AVI_packet_rx_event_handler(msg_obj_t *msg);
int DSRC_send_timer_handler(buffer_ring_t *buffer);
void OBU_j2735_tx(DSRCmsgID magId, void *data);
int Is_Heartbeat(msg_obj_t *msg);
/* Return codes of packet processing */
typedef enum packet_processing_err {
    PACKET_NOT_J2735 = 1,
    PACKET_PROCESSING_ACCEPT = 0,
    PACKET_INVALID_PACKET_LEN = -1,
    PACKET_INVALID_DEVICE_TYPE = -2,
    PACKET_INVALID_RSU_NAME = -3,
    PACKET_INVALID_SEVICE_ID = -4,
    PACKET_INVALID_VEHICLE_TYPE = -5
} packet_processing_err_t;

#endif