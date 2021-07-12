#include "server.h"

#include "ae_timer_event.h"
#include "msg_queue.h"
#include "network.h"
#define USING_DEFAULT 1
#ifdef USING_DEFAULT
const int CONFIG_BINDADDR_MAX = 16;
const int CONFIG_PORT = 10000;
#define CONFIG_TCP_ENABLE 1
#define CONFIG_UDP_ENABLE 2
#endif
typedef struct Server comm_server_t;
typedef struct Broker comm_broker_t;
comm_server_t RSU_server;
pthread_t com_layer_thread;
/* Functions managing dictionary of callbacks for pub/sub. */
static uint64_t callback_hash(const void *key)
{
	return dictGenHashFunction((unsigned char *)key, strlen((char *)key));
}
static int callback_key_compare(void *private_data, const void *key1, const void *key2)
{
	/*string compare*/
	DICT_NOTUSED(private_data);
	int l1 = strlen((char *)key1);
	int l2 = strlen((char *)key2);
	if (l1 != l2)
		return 0;
	return memcmp(key1, key2, l1) == 0;
}
static void callback_key_destructor(void *private_data, void *key)
{
	DICT_NOTUSED(private_data);
	free(key);
}
static dict_type client_callback_dict = {
    callback_hash,
    NULL,
    NULL,
    callback_key_compare,
    callback_key_destructor,
    NULL};

int com_layer_init(char *config_file_path)
{
	if (config_file_path) {
		config_handle(config_file_path);
	}
	/*if config_file_path is null,using default config setting*/
	if (init_server(&RSU_server) == SERVER_ERR_INIT)
		return SERVER_ERR_INIT;
	pthread_create(&com_layer_thread, NULL, (void *)start_server, (void *)(&RSU_server));
}
int init_server(comm_server_t *server)
{
	server->broker = malloc(sizeof(*(server->broker)));
	if (!server->broker)
		return SERVER_ERR_INIT;
	else
		server->broker->server = server;
	server->port = CONFIG_PORT + 1;
	server->bind_addr = NULL;
	server->setsize = 1024;
	server->dispatch_com_id = 0;
	server->el = ae_create_event_loop(server->setsize);
	if (!server->el) {
		fprintf(stderr, "Create event-loop error\n");
		return SERVER_ERR_INIT;
	}
	server->el->server = (comm_server_t *)server;
	server->listen_TCP_fd = net_TCP_server(server->err_info, server->port, server->bind_addr, server->backlog);
	if (server->listen_TCP_fd != NET_ERR) {
		if (net_non_block(server->err_info, server->listen_TCP_fd) == NET_ERR)
			return SERVER_ERR_INIT;
	}
	else {
		fprintf(stderr, "Open port %d error: %s\n", server->port, server->err_info);
		return SERVER_ERR_INIT;
	}
	server->listen_UDP_fd = net_UDP_server(server->err_info, server->port, server->bind_addr);
	if (server->listen_UDP_fd != NET_ERR) {
		if (net_non_block(server->err_info, server->listen_UDP_fd) == NET_ERR)
			return SERVER_ERR_INIT;
	}
	else {
		fprintf(stderr, "Open port %d error: %s\n", server->port, server->err_info);
		return SERVER_ERR_INIT;
	}
	/* add listen_TCP_fd to epoll instance，setting to callback function to  accept TCP Handler */
	if (ae_create_comm_event(server->el, server->listen_TCP_fd, AE_READABLE, conn_accept_TCP_handler, server) != AE_ERR) {
		char conn_info[64];
		net_format_sock(server->listen_TCP_fd, conn_info, sizeof(conn_info));
		printf("TCP:listen on: %s\n", conn_info);
	}
	else {
		fprintf(stderr, "Fail to add listener event on %d\n", server->listen_TCP_fd);
		return SERVER_ERR_INIT;
	}
	/* add listen_UDP_fd to epoll instance，setting to callback function to  accept UDP Handler */
	if (ae_create_comm_event(server->el, server->listen_UDP_fd, AE_READABLE, conn_accept_UDP_handler, server) != AE_ERR) {
		char conn_info[64];
		net_format_sock(server->listen_UDP_fd, conn_info, sizeof(conn_info));
		printf("UDP:listen on: %s\n", conn_info);
	}
	else {
		fprintf(stderr, "Fail to add listener event on %d\n", server->listen_UDP_fd);
		return SERVER_ERR_INIT;
	}
	/*Time event creation*/
	/* Create the timer callback, this is our way to process many background
     * operations incrementally, like clients timeout, logging and so forth. */
	if (ae_create_time_event(server->el, 500000, on_cloud_disconnected, &server->cloud_expired_id, NULL) == AE_ERR) {
		fprintf(stderr, "Fail to create timing event \n");
		return SERVER_ERR_INIT;
	}
	if (ae_create_time_event(server->el, 3000, time_print_cur_time, &server->timer_logged_id, NULL) == AE_ERR) {
		fprintf(stderr, "Fail to create timing event \n");
		return SERVER_ERR_INIT;
	}
	/*Init broker*/
	server->broker->client_dict = dict_create(&client_callback_dict, NULL);
	if (server->broker->client_dict == NULL) return SERVER_ERR_INIT;

	return SERVER_OK;
}
int start_server(comm_server_t *server)
{
	ae_main(server->el);
	ae_delete_event_loop(server->el);
}
void comm_packet_enqueue(client_t *client, uint8_t from_type)
{
	struct msg_obj *_msg_obj = msg_obj_create(client->read_buffer->buff, from_type, client->com_id);
	if (_msg_obj != NULL) {
		msg_queue_enqueue(_msg_obj);
	}
}
int comm_dict_add(dict *ht, client_t *client, int com_id)
{
	if (com_id < 0)
		return DICT_ERR;
	char str[4];
	snprintf(str, sizeof(str), "%d", com_id);
	return dict_add(ht, strdup(str), client);
}
int comm_dict_delete(dict *ht, int com_id)
{
	if (com_id < 0)
		return DICT_ERR;
	char str[4];
	snprintf(str, sizeof(str), "%d", com_id);
	char *pstr = strdup(str);
	int ret = dict_delete(ht, pstr);
	free(pstr);
	return ret;
}
client_t *comm_dict_find(dict *ht, int com_id)
{
	client_t *client;
	char str[4];
	snprintf(str, sizeof(str), "%d", com_id);
	char *pstr = strdup(str);
	dict_entry *de = dict_find(ht, pstr);
	if (de == NULL) 
	{
		if (pstr != NULL)
			free(pstr);
		return NULL;
	}
	client = (client_t *)de->val;
	if (pstr != NULL)
		free(pstr);
	return client;
}
client_t *conn_alloc_client(uint8_t client_conn_type)
{
	client_t *client = malloc(sizeof(client_t));
	if (!client)
		goto err1;
	client->el = NULL;
	client->fd = INVALID_FD; /*default fd = -1*/
	client->read_buffer = alloc_buffer();
	client->write_buffer = alloc_buffer();
	if (client->read_buffer == NULL || client->write_buffer == NULL) {
		goto err1;
	}
	if (client_conn_type == UDP_HANDLE) {
		client->handle = alloc_udp_client_handle();
		if (client->handle->type != UDP_HANDLE)
			goto err1;
		if (client->handle == NULL)
			goto err1;
		if (client->handle->recv_fn == NULL || client->handle->send_fn == NULL)
			goto err1;
	}
	else if (client_conn_type == TCP_HANDLE) {
		client->handle = alloc_tcp_client_handle();
		if (client->handle == NULL)
			goto err1;
		if (client->handle->type != TCP_HANDLE)
			goto err1;
		if (client->handle->recv_fn == NULL || client->handle->send_fn == NULL)
			goto err1;
	}
	else
		goto err1;
	return client;
err1:
	if (client) {
		free(client->read_buffer);
		free(client->write_buffer);
		if (client->handle->_ae_handle != NULL)
			free(client->handle->_ae_handle);
		if (client->handle != NULL)
			free(client->handle);
		free(client);
	}
	return NULL;
}
void conn_free_client(client_t *client)
{
	// timestamp
    time_t rawtime;
    struct tm *info;
    char buf[20];
    time(&rawtime);
    info = localtime( &rawtime );
    strftime(buf, 20, "%Y-%m-%d %H:%M:%S", info);
    

	if (client) {
		if (client->fd > 0) {
			ae_delete_comm_event(client->el, client->fd, AE_READABLE);
			ae_delete_comm_event(client->el, client->fd, AE_WRITABLE);
			close(client->fd);
		}
		free(client->read_buffer->buff);
		free(client->read_buffer);
		free(client->write_buffer->buff);
		free(client->write_buffer);
		free(client->handle->_ae_handle);
		free(client->handle);
		free(client);
	}
}
void conn_accept_TCP_handler(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	int cfd, cport;
	char ip_addr[128] = {0};
	comm_server_t *serv = (comm_server_t *)event_loop->server;

	cfd = net_TCP_accept(serv->err_info, fd, ip_addr, sizeof ip_addr, &cport);
	if (cfd == -1)
		return;
	printf("Connected from %s:%d\n", ip_addr, cport);
	if (net_non_block(NULL, cfd) < 0) {
		fprintf(stderr, "fail to set client fd to be nonblock: %d\n", fd);
		close(fd);
		return;
	}
	/*delete cloud expired event*/
	if (ae_delete_time_event(serv->el, serv->cloud_expired_id) == AE_ERR) {
		fprintf(stderr, "delete expired timer event error %d\n", serv->cloud_expired_id);
	}
	else {
		/*todo:should using logging to record*/
		printf("cloud reconnect!\n");
	}
	client_t *client = conn_alloc_client(TCP_HANDLE);
	if (!client) {
		printf("alloc client error...close socket\n");
		close(fd);
		return;
	}
	client->el = event_loop;
	client->fd = cfd;
	client->com_id = serv->dispatch_com_id++;
	int retval = comm_dict_add(serv->broker->client_dict, client, client->com_id);
	if (ae_create_comm_event(event_loop, cfd, AE_READABLE, conn_read_from_client_TCP, client) == AE_ERR) {
		fprintf(stderr, "create socket readable event error, close fd: %d\n", fd);
		comm_dict_delete(serv->broker->client_dict, client->com_id);
		conn_free_client(client);
	}
}
void conn_accept_UDP_handler(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	int cfd, cport;
	char ip_addr[128] = {0};
	comm_server_t *serv = (comm_server_t *)event_loop->server;
	char buf[MAX_BUF_LEN] = {0};
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	cfd = net_UDP_accept(serv->err_info, serv->port, buf, fd, MAX_BUF_LEN);
	if (cfd >= 0) {
		client_t *client = conn_alloc_client(UDP_HANDLE);
		if (!client) {
			printf("alloc client error...close socket\n");
			close(fd);
			return;
		}
		client->el = event_loop;
		client->fd = cfd;
		client->com_id = serv->dispatch_com_id++;
		int retval = comm_dict_add(serv->broker->client_dict, client, client->com_id);
		ae_prepare_for_enqueue_early(client, buf);
		comm_packet_enqueue(client, FROM_DSRC);
		if (ae_create_comm_event(event_loop, cfd, AE_READABLE, conn_read_from_client_UDP, client) == AE_ERR) {
			fprintf(stderr, "create socket readable event error, close fd: %d\n", fd);
			comm_dict_delete(serv->broker->client_dict, client->com_id);
			conn_free_client(client);
		}
	}
}
void conn_read_from_client_TCP(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	client_t *client = (client_t *)clientData;
	comm_server_t *serv = (comm_server_t *)event_loop->server;
	ssize_t readn = client->handle->recv_fn(client);
	if (readn > 0) {
		comm_packet_enqueue(client, FROM_CLOUD);
	}
	else if (readn == 0 || readn == -1) {
		printf("client disconnect, close it.\n");
		comm_dict_delete(serv->broker->client_dict, client->com_id);
		conn_free_client(client);
		if (ae_create_time_event(serv->el, 500000, on_cloud_disconnected, &serv->cloud_expired_id, NULL) == AE_ERR) {
			fprintf(stderr, "Fail to create timing event \n");
		}
	}
}
void conn_write_to_client_TCP(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	client_t *client = (client_t *)clientData;
	buffer_t *wbuffer = client->write_buffer;
	int data_size = (int)get_buffer_size(wbuffer);
	if (data_size == 0) {
		ae_delete_comm_event(client->el, client->fd, AE_WRITABLE);
		return;
	}
	int written = client->handle->send_fn(client);

	if (get_buffer_size(wbuffer) == 0)
		ae_delete_comm_event(client->el, client->fd, AE_WRITABLE);
}
void conn_read_from_client_UDP(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	client_t *client = (client_t *)clientData;
	comm_server_t *serv = (comm_server_t *)event_loop->server;
	ssize_t readn = client->handle->recv_fn(client);
	if (readn > 0) {
		comm_packet_enqueue(client, FROM_DSRC);
	}
}
void conn_write_to_client_UDP(struct ae_event_loop *event_loop, int fd, void *clientData, int mask)
{
	comm_server_t *serv = (comm_server_t *)event_loop->server;
	client_t *client = (client_t *)clientData;
	buffer_t *wbuffer = client->write_buffer;

	int data_size = (int)get_buffer_size(wbuffer);
	if (data_size == 0) {
		ae_delete_comm_event(client->el, client->fd, AE_WRITABLE);
		return;
	}
	ssize_t send_n = client->handle->send_fn(client);
	if (get_buffer_size(wbuffer) == 0) {
		ae_delete_comm_event(client->el, client->fd, AE_WRITABLE);
	}
}
void config_handle(char *path)
{
}