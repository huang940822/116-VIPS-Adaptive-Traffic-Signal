#ifndef TIB_PACKET_TX_H
#define TIB_PACKET_TX_H
#include <signal.h>

void MAP_packet_tx(__sigval_t value);
void *SPaT_packet_tx_loop();
void TIB_send_ack();

#endif