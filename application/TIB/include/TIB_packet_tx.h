#ifndef TIB_PACKET_TX_H
#define TIB_PACKET_TX_H

#include "j2735_BroadcastList.h"
typedef struct J2735_msg_obj J2735_msg_obj_t;

void *MAP_packet_tx_loop();
void *SPaT_packet_tx_loop();
void *general_packet_tx_loop();
void TIB_send_ack();
#endif