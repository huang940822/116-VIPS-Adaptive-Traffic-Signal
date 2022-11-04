#ifndef MAP_PACKET_TX_H
#define MAP_PACKET_TX_H
#include <signal.h>

void MAP_packet_tx(__sigval_t value);
void MAP_send_ack();

#endif