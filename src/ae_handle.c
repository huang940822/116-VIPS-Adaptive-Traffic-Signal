#include "ae_handle.h"

#include <assert.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_USE_MODULE(MIDDLEWARE);

pthread_mutex_t mutex_client_write = PTHREAD_MUTEX_INITIALIZER;


uint32_t ae_retrieve_packet_len(unsigned char *packet)
{
    len_converter conv;
    memcpy(&conv.bytes, packet, sizeof(uint32_t));
    /*A robust way to do endian conversion*/
    unsigned char dest[sizeof(uint32_t)];
    for (int k = 0; k < sizeof(uint32_t); k++)
        dest[k] = conv.bytes[sizeof(uint32_t) - k - 1];
    memcpy(&conv.bytes, dest, sizeof(uint32_t));
    return conv.val;
}
int udp_check_for_sending(udp_handle_t handle)
{
    if (handle.max_msg_len == -1)
        return HANDLE_ERR;
    return HANDLE_OK;
}
int udp_type_check(uint8_t type)
{
    return type == UDP_HANDLE ? HANDLE_OK : HANDLE_ERR;
}

//這裡把要送的資料copy到client物件
void ae_prepare_for_sending(client_t *client,
                            unsigned char *buf,
                            size_t send_len)
{
    buffer_t *buffer = alloc_buffer();
    memset(buffer->buff, 0, HANDLE_MSG_LEN);
    memcpy(buffer->buff, buf, send_len);
    increase_buffer_size(buffer, send_len);
    while (!buff_ring_push(buffer, client->write_buffer))
        ;
}
void ae_prepare_for_enqueue_early(client_t *client, unsigned char *buf)
{
    memset(client->read_buffer->buff, 0, HANDLE_MSG_LEN);
    memcpy(client->read_buffer->buff, buf, HANDLE_MSG_LEN);
    increase_buffer_size(client->read_buffer, HANDLE_MSG_LEN);
}

size_t udp_send(client_t *client)
{
    uint8_t _type = client->handle->type;
    udp_handle_t udp_handle = client->handle->_ae_handle->_udp_handle;
    if (udp_type_check(_type) == HANDLE_ERR)
        return HANDLE_ERR;
    if (udp_check_for_sending(udp_handle) == HANDLE_ERR)
        return HANDLE_ERR;

    buffer_t *buff = buff_ring_pop(client->write_buffer);
    if (buff == NULL)
        return HANDLE_ERR;

    size_t written = send(client->fd, buff->buff, buff->size, 0);
    if (written != buff->size)
        LOG_MSG_TRACE("send error");

    free_buffer(buff);
    return written;
}
size_t udp_recv(client_t *client)
{
    /*type check*/
    uint8_t _type = client->handle->type;
    udp_handle_t udp_handle = client->handle->_ae_handle->_udp_handle;
    if (udp_type_check(_type) == HANDLE_ERR)
        return HANDLE_ERR;
    memset(client->read_buffer->buff, 0, HANDLE_MSG_LEN);
    size_t readn =
        recv(client->fd, client->read_buffer->buff, udp_handle.max_msg_len, 0);
    client->read_buffer->size = readn;
    if (readn == -1) {
        LOG_MSG_TRACE("UDP: EAGAIN");
    }
    return readn;
}
client_handle_t *alloc_udp_client_handle()
{
    client_handle_t *c_handle = malloc(sizeof(client_handle_t));
    if (!c_handle)
        goto err1;
    c_handle->type = UDP_HANDLE;
    c_handle->_ae_handle = malloc(sizeof(ae_handle));
    if (c_handle->_ae_handle == NULL)
        goto err2;

    c_handle->_ae_handle->_udp_handle.max_msg_len = HANDLE_MSG_LEN;
    /*set function pointer*/
    c_handle->send_fn = udp_send;
    c_handle->recv_fn = udp_recv;
    return c_handle;
err2:
    free(c_handle);
    return NULL;
err1:
    return NULL;
}
int tcp_check_for_sending(tcp_handle_t handle)
{
    if (handle.max_msg_len == -1)
        return HANDLE_ERR;
    return HANDLE_OK;
}
int tcp_type_check(uint8_t type)
{
    return type == TCP_HANDLE ? HANDLE_OK : HANDLE_ERR;
}
size_t tcp_send(client_t *client)
{
    /*type check*/
    uint8_t _type = client->handle->type;
    tcp_handle_t tcp_handle = client->handle->_ae_handle->_tcp_handle;
    if (tcp_type_check(_type) == HANDLE_ERR)
        return HANDLE_ERR;
    if (tcp_check_for_sending(tcp_handle) == HANDLE_ERR)
        return HANDLE_ERR;
    buffer_t *buff = buff_ring_pop(client->write_buffer);
    if (buff == NULL)
        return HANDLE_ERR;

    pthread_mutex_lock(&mutex_client_write);
    size_t written = net_TCP_write(client->fd, buff->buff, buff->size);
    pthread_mutex_unlock(&mutex_client_write);

    if (written != buff->size) {
        LOG_MSG_TRACE("ERR:written %ld v.s expected written %ld", written,
                buff->size);
    }

    //把已經送出去的長度減掉嗎？
    // decrease_buffer_size(client->write_buffer, written);

    return written;
}
void test_ae_check_packet(unsigned char *packet, uint32_t packet_len)
{
    // packet dump
    uint32_t packet_sum = 0;
    for (int i = sizeof(uint32_t); i < packet_len; i++) {
        if ((packet[i] - '0') != 1)
            LOG_MSG_TRACE("i=%d err %u %c", i, packet[i] - '0', packet[i]);
        packet_sum += (packet[i] - '0');
    }
    LOG_MSG_TRACE("packet sum = %u", packet_sum);
    assert(packet_len == packet_sum + sizeof(uint32_t));
}
void tcp_handle_recv_init(client_t *client)
{
    memset(client->read_buffer->buff, 0, HANDLE_MSG_LEN);
    client->handle->_ae_handle->_tcp_handle.expected_msg_len = 0;
    client->handle->_ae_handle->_tcp_handle.cur_msg_len = 0;
    client->handle->_ae_handle->_tcp_handle.is_pending = true;
}
bool tcp_is_pending(client_t *client)
{
    return client->handle->_ae_handle->_tcp_handle.is_pending;
}
bool tcp_recv_is_done(client_t *client)
{
    if (client->handle->_ae_handle->_tcp_handle.expected_msg_len == 0)
        return false;
    if (client->handle->_ae_handle->_tcp_handle.cur_msg_len >
        client->handle->_ae_handle->_tcp_handle.max_msg_len)
        return false;
    return client->handle->_ae_handle->_tcp_handle.expected_msg_len ==
           client->handle->_ae_handle->_tcp_handle.cur_msg_len;
}
size_t tcp_recv(client_t *client)
{
    /*type check*/
    uint8_t _type = client->handle->type;
    tcp_handle_t tcp_handle = client->handle->_ae_handle->_tcp_handle;
    if (tcp_type_check(_type) == HANDLE_ERR)
        return HANDLE_ERR;
    if (!tcp_is_pending(client)) {
        tcp_handle_recv_init(client);
        size_t readn = recv(client->fd, client->read_buffer->buff,
                            tcp_handle.max_msg_len, 0);
        uint32_t packet_len = ae_retrieve_packet_len(client->read_buffer->buff);
        if (readn == -1)
            return HANDLE_ERR;
        client->handle->_ae_handle->_tcp_handle.expected_msg_len = packet_len;
        client->handle->_ae_handle->_tcp_handle.cur_msg_len = readn;
        LOG_MSG_TRACE("packet length = %u", packet_len);
        // LOG_MSG_TRACE("In %s :recv:%ld", __func__, readn);
    } else {
        size_t remain =
            client->handle->_ae_handle->_tcp_handle.expected_msg_len -
            client->handle->_ae_handle->_tcp_handle.cur_msg_len;
        size_t readn =
            recv(client->fd,
                 client->read_buffer->buff +
                     client->handle->_ae_handle->_tcp_handle.cur_msg_len,
                 remain, 0);
        if (readn == -1 || readn == 0)
            return HANDLE_ERR;
        client->handle->_ae_handle->_tcp_handle.cur_msg_len += readn;
        LOG_MSG_TRACE("Remaining recv %ld", readn);
    }
    if (tcp_recv_is_done(client)) {
        LOG_MSG_TRACE("done");
        client->handle->_ae_handle->_tcp_handle.is_pending = false;
        assert(client->handle->_ae_handle->_tcp_handle.expected_msg_len ==
               client->handle->_ae_handle->_tcp_handle.cur_msg_len);
        // test_ae_check_packet(client->read_buffer->buff,
        // client->handle->_ae_handle->_tcp_handle.expected_msg_len);
        return client->handle->_ae_handle->_tcp_handle.expected_msg_len;
    }

    return HANDLE_RECV_NOT_DONE;
}
client_handle_t *alloc_tcp_client_handle()
{
    client_handle_t *c_handle = malloc(sizeof(client_handle_t));
    if (c_handle == NULL)
        goto err1;
    c_handle->type = TCP_HANDLE;
    c_handle->_ae_handle = malloc(sizeof(ae_handle));
    if (c_handle->_ae_handle == NULL)
        goto err2;
    c_handle->_ae_handle->_tcp_handle.max_msg_len = HANDLE_MSG_LEN;
    c_handle->_ae_handle->_tcp_handle.cur_msg_len = 0;
    c_handle->_ae_handle->_tcp_handle.expected_msg_len = 0;
    c_handle->_ae_handle->_tcp_handle.is_pending = false;
    /*set function pointer*/
    c_handle->send_fn = tcp_send;
    c_handle->recv_fn = tcp_recv;
    return c_handle;
err2:
    free(c_handle->_ae_handle);
    free(c_handle);
    return NULL;
err1:
    return NULL;
}
