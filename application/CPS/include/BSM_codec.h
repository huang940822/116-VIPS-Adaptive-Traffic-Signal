#ifndef BSM_H
#define BSM_H
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ObstacleList.h"
#include "asn1defs_if.h"
#include "j2735_codec.h"

void dump_mem(void *data, int len);
void bsm_print(BasicSafetyMessage *bsm);
int bsm_encode(uint8_t **tx_buf, int *tx_buf_len, Obstacle *obstacle);
int bsm_encode_reg(uint8_t **tx_buf,
                   size_t *tx_buf_len,
                   ObstacleList *obstaclelist);
void bsm_decode(uint8_t *rx_buf, int rx_buf_len);

#endif
