#ifndef BSM_H
#define BSM_H
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <pthread.h>

#include "j2735_codec.h"
#include "asn1defs_if.h"
#include "asn1defs_if.h"
#include "ObstacleList.h"

void dump_mem(void *data, int len);
void bsm_print(BasicSafetyMessage *bsm);
int bsm_encode(uint8_t **tx_buf, int *tx_buf_len, Obstacle *obstacle);
int bsm_encode_reg(uint8_t **tx_buf, int *tx_buf_len, ObstacleList *obstaclelist);
void bsm_decode(uint8_t *rx_buf, int rx_buf_len);

#endif
