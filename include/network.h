#ifndef NETWORK_H
#define NETWORK_H
#define NET_OK 0
#define NET_ERR -1
#define NET_ERR_LEN 256
#define NET_NONE 0
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ip.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#define SMART_AVI_PORT 12345
#define Heartbeat_PORT 10001

int net_set_reuse_addr(char *err, int fd);
int net_set_reuse_port(char *err, int fd);
int net_set_mss_value(char *err, int fd, int mss_val);
int net_set_recv_buf_size(char *err, int fd, int recv_size);
int net_set_send_buf_size(char *err, int fd, int send_size);
int net_set_tcp_no_nagle(char *err, int fd);
int net_non_block(char *err, int fd);
int net_TCP_server(char *err, int port, char *bindaddr, int backlog);
int net_TCP_accept(char *err, int serversock, char *ip, size_t ip_len, int *port);
int net_UDP_accept(char *err, int port, char *recv_buf, int listen_fd, int max_buf_len, int *Is_smart_AVI, int *Is_Heartbeat, struct sockaddr_in *heartbeat_addr);
int net_TCP_read(int fd, char *buf, int count);
size_t net_TCP_write(int fd, char *buf, int count);
int net_UDP_server(char *err, int port, char *bindaddr);
int net_format_peer(int fd, char *buf, size_t buf_len);
int net_format_sock(int fd, char *buf, size_t buf_len);
int net_listen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog);
int net_generic_accept(char *err, int serversock, struct sockaddr *sa, socklen_t *len);
int net_TCP_client(char *err, char *server_addr, int server_port);
#endif