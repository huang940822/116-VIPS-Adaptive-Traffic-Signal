#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "buffer.h"
#include "network.h"
#define HANDLE_ERR -1
#define HANDLE_OK 0
#define TCP_HANDLE 1
#define UDP_HANDLE 2
#define ZMQ_HANDLE 3
#define HANDLE_RECV_NOT_DONE -2
#define HANDLE_MSG_LEN 25600
#define INVALID_FD -1
#define INVALID_PORT -2
#define CLIENT_RING_MAX 10
typedef struct Client client_t;
typedef struct _tcp_handle {
	size_t cur_msg_len;
	size_t expected_msg_len;
	size_t max_msg_len;
	bool is_pending;
} tcp_handle_t;

typedef struct _udp_handle {
	size_t max_msg_len;
} udp_handle_t;

typedef union ae_handle {
	tcp_handle_t _tcp_handle;
	udp_handle_t _udp_handle;
} ae_handle;

typedef struct client_handle {
	uint8_t type;
	union ae_handle *_ae_handle;
	size_t (*send_fn)(client_t *);
	size_t (*recv_fn)(client_t *);
} client_handle_t;

typedef struct Client {
	struct ae_event_loop *el;
	int fd;
	int com_id;
	buffer_t *read_buffer;
	buffer_ring_t *write_buffer;
	client_handle_t *handle;
} client_t;

typedef union len_converter {
	unsigned char bytes[sizeof(uint32_t)];
	uint32_t val;
} len_converter;

void test_ae_check_packet(unsigned char *packet, uint32_t packet_len);

uint32_t ae_retrieve_packet_len(unsigned char *packet);

void ae_prepare_for_sending(client_t *client, unsigned char *buf, size_t send_len);

void ae_prepare_for_enqueue_early(client_t *client, unsigned char *buf);

int udp_type_check(uint8_t type);

size_t udp_send(client_t *client);

size_t udp_recv(client_t *client);

client_handle_t *alloc_udp_client_handle();

int tcp_type_check(uint8_t type);

size_t tcp_send(client_t *client);

void tcp_handle_recv_init(client_t *client);

bool tcp_is_pending(client_t *client);

bool tcp_recv_is_done(client_t *cleint);

size_t tcp_recv(client_t *client);

client_handle_t *alloc_tcp_client_handle();

#endif