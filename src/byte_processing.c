#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "byte_processing.h"

uint32_t htonf(float f)
{
  uint32_t *p_tmpu32 = (uint32_t *) &f;
  return htonl(*p_tmpu32);
}

float ntohf(uint32_t p)
{
  uint32_t tmpu32 = ntohl(p);
  float *p_tmpfloat = (float *)&tmpu32;
  return (*p_tmpfloat);
}

void uint16_t2byte(uint16_t input, unsigned char *output)
{
    uint16_t NBO_num = htons(input);
    memcpy(output, &NBO_num, 2);
    return;
}

uint16_t byte2uint16_t(unsigned char *input)
{
    uint16_t NBO_num = ((uint16_t)input[0] << 8) | (uint16_t)input[1];
    return NBO_num;
}

void uint32_t2byte(uint32_t input, unsigned char *output)
{
    uint32_t NBO_num = htonl(input);
    memcpy(output, &NBO_num, 4);
    return;
}

uint32_t byte2uint32_t(unsigned char *input)
{
    uint32_t NBO_num = ((uint32_t)input[0] << 24) | ((uint32_t)input[1] << 16) | ((uint32_t)input[2] << 8) | (uint32_t)input[3];
    return NBO_num;
}
int32_t byte2int32_t(unsigned char *input)
{
    int32_t NBO_num = ((int32_t)input[0] << 24) | ((int32_t)input[1] << 16) | ((int32_t)input[2] << 8) | (int32_t)input[3];
    return NBO_num;
}
void float2byte(float input, unsigned char *output)
{
    uint32_t NBO_num = htonf(input);
    memcpy(output, &NBO_num, 4);
    return;
}

float byte2float(unsigned char *input)
{
    uint32_t NBO_num = ((uint32_t)input[0] << 24) | ((uint32_t)input[1] << 16) | ((uint32_t)input[2] << 8) | (uint32_t)input[3];
    float *output = (float *)&NBO_num;
    return *output;
}

double byte2double(unsigned char *input)
{
    uint64_t NBO_num = ((uint64_t)input[0] << 56) | ((uint64_t)input[1] << 48) | ((uint64_t)input[2] << 40) | ((uint64_t)input[3] << 32)
                        | ((uint64_t)input[4] << 24) | ((uint64_t)input[5] << 16) | ((uint64_t)input[6] << 8) | ((uint64_t)input[7]);
    double *output = (double *)&NBO_num;
    return *output;
}

/////////////////////////////////////////////

void read_char(char *dst, msg_buf_t *buf, int len)
{
    memcpy(dst, &buf->content[buf->index], len);
    buf->index += len;
}

void read_int8_t(int8_t *dst, msg_buf_t *buf)
{
    *dst = (int8_t)buf->content[buf->index];
    buf->index += 1;
}

void read_uint8_t(uint8_t *dst, msg_buf_t *buf)
{
    *dst = (uint8_t)buf->content[buf->index];
    buf->index += 1;
}

void read_uint16_t(uint16_t *dst, msg_buf_t *buf)
{
    *dst = byte2uint16_t(&buf->content[buf->index]);
    buf->index += 2;
}

void read_uint32_t(uint32_t *dst, msg_buf_t *buf)
{
    *dst = byte2uint32_t(&buf->content[buf->index]);
    buf->index += 4;
}

void read_float(float *dst, msg_buf_t *buf)
{
    *dst = byte2float(&buf->content[buf->index]);
    buf->index += 4;
}

void write_char(char *src, msg_buf_t *buf, int src_len, int max_len)
{
    memset(&buf->content[buf->index], ' ', max_len);
    memcpy(&buf->content[buf->index], src, src_len);
    buf->index += max_len;
}

void write_uint8_t(uint8_t src, msg_buf_t *buf)
{
    memcpy(&buf->content[buf->index], &src, 1);
    buf->index += 1;
}

void write_uint16_t(uint16_t src, msg_buf_t *buf)
{
    uint16_t2byte(src, &buf->content[buf->index]);
    buf->index += 2;
}

void write_uint32_t(uint32_t src, msg_buf_t *buf)
{
    uint32_t2byte(src, &buf->content[buf->index]);
    buf->index += 4;
}

void write_float(float src, msg_buf_t *buf)
{
    float2byte(src, &buf->content[buf->index]);
    buf->index += 4;
}