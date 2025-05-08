#ifndef SERVER_H
#define SERVER_H
#define SERVER_OK 0
#define SERVER_ERR_INIT -1
#define SERVER_CREATE_EV_ERR -2
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "typedefine.h"
#include "ae_event.h"
#include "ae_handle.h"
#include "buffer.h"
#include "dict.h"
#include "list.h"
#define MAX_BUF_LEN 25600
#define FROM_CLOUD 1
#define FROM_DSRC 2
#define FROM_SMART_AVI 3
#define FROM_PEDESTRIAN 4


# if CPS_DEBUG > 0
    int cnt;
    double tsmp[CPS_DEBUG];
# endif

void config_handle(char *path);
struct Broker {
    struct Server *server; /*point back to server*/
    dict *client_dict;
};
struct Server {
    char *config_file;
    char err_info[256];
    int32_t port;
    struct ae_event_loop *el;
    char *bind_addr;
    int32_t listen_TCP_fd;
    int32_t listen_UDP_fd;
    int32_t listen_SMART_AVI_fd;
    /* new type id: pedestrian */
    int32_t listen_Pedestrian_fd;
    int32_t cloud_expired_id;
    int32_t timer_logged_id;
    int32_t dispatch_com_id;
    int32_t setsize;
    int32_t listen_fd_cnt;
    int32_t backlog;
    struct Broker *broker;
};

extern struct Server RSU_server;

extern pthread_t com_layer_thread;

int com_layer_init(char *config_file_path);

int init_server(struct Server *server);

int start_server(struct Server *server);

void conn_accept_TCP_handler(ae_event_loop *eventLoop,
                             int fd,
                             void *clientData,
                             int mask);

void conn_read_from_client_TCP(ae_event_loop *event_loop,
                               int fd,
                               void *clientData,
                               int mask);

void conn_write_to_client_TCP(ae_event_loop *event_loop,
                              int fd,
                              void *clientData,
                              int mask);

client_t *conn_alloc_client(uint8_t client_conn_type);

void conn_free_client(client_t *client);

void conn_accept_UDP_handler(ae_event_loop *event_loop,
                             int fd,
                             void *clientData,
                             int mask);

void conn_accept_Smart_AVI_handler(ae_event_loop *event_loop,
                                   int fd,
                                   void *clientData,
                                   int mask);
void conn_accept_Pedestrian_handler(ae_event_loop *event_loop,
                                    int fd,
                                    void *clientData,
                                    int mask);

void conn_read_from_client_UDP(ae_event_loop *event_loop,
                               int fd,
                               void *clientData,
                               int mask);

void conn_read_from_SMART_AVI_UDP(ae_event_loop *event_loop,
                                  int fd,
                                  void *clientData,
                                  int mask);

void conn_read_from_Pedestrian_UDP(ae_event_loop *event_loop,
                                    int fd,
                                    void *clientData,
                                    int mask);
                                    
void conn_write_to_client_UDP(ae_event_loop *event_loop,
                              int fd,
                              void *clientData,
                              int mask);

void comm_packet_enqueue(client_t *client, uint8_t from_type);

int comm_dict_add(dict *ht, client_t *client, int com_id);

int comm_dict_delete(dict *ht, int com_id);

client_t *comm_dict_find(dict *ht, int com_id);

int comm_create_OBU_client(struct ae_event_loop *event_loop,
                           int listen_fd,
                           char *err,
                           struct sockaddr_in client_addr);
#endif
