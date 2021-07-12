#ifndef BYTE_PROCESSING_H
#define BYTE_PROCESSING_H
#include "typedefine.h"

void uint16_t2byte(uint16_t input, unsigned char *output);
uint16_t byte2uint16_t(unsigned char *input);

void uint32_t2byte(uint32_t input, unsigned char *output);
uint32_t byte2uint32_t(unsigned char *input);

void float2byte(float input, unsigned char *output);
float byte2float(unsigned char *input);

void read_char(char *dst, msg_buf_t *buf, int len);
void read_int8_t(int8_t *dst, msg_buf_t *buf);
void read_uint8_t(uint8_t *dst, msg_buf_t *buf);
void read_uint16_t(uint16_t *dst, msg_buf_t *buf);
void read_uint32_t(uint32_t *dst, msg_buf_t *buf);
void read_float(float *dst, msg_buf_t *buf);

void write_char(char *src, msg_buf_t *buf, int src_len, int max_len);
void write_uint8_t(uint8_t src, msg_buf_t *buf);
void write_uint16_t(uint16_t src, msg_buf_t *buf);
void write_uint32_t(uint32_t src, msg_buf_t *buf);
void write_float(float src, msg_buf_t *buf);

#endif