#include "com_io.h"
#include <sys/time.h>

#include "ae_handle.h"
#include "buffer.h"
#include "dict.h"
#include "log.h"
#include "server.h"

int com_send(int com_id, unsigned char *buf, size_t send_len)
{  // printf("com_send com_id: %d\n", com_id);
    struct ae_event_loop *event_loop = RSU_server.el;
    client_t *client = comm_dict_find(RSU_server.broker->client_dict, com_id);
    if (client == NULL) {
        log_file_write_fatal_error("com_send dict not found, com_id: %d", com_id);
        return COM_IO_ERR;
    }
    ae_prepare_for_sending(client, buf, send_len);
    if (client->handle->type == UDP_HANDLE) {
        if (ae_create_comm_event(event_loop, client->fd, AE_WRITABLE,
                                 conn_write_to_client_UDP, client) == AE_ERR) {
            fprintf(stderr, "UDP create writable comm event error, close fd: %d\n", client->fd);
            log_file_write_fatal_error("UDP create writable comm event error, close fd: %d, com_id: %d", client->fd, com_id);

            comm_dict_delete(RSU_server.broker->client_dict, client->com_id);
            conn_free_client(client);
            return COM_IO_ERR;
        }
    } else if (client->handle->type == TCP_HANDLE) {
        if (ae_create_comm_event(event_loop, client->fd, AE_WRITABLE,
                                 conn_write_to_client_TCP, client) == AE_ERR) {
            fprintf(stderr, "TCP create writable comm event error, close fd: %d\n", client->fd);
            log_file_write_fatal_error("TCP create writable comm event error, close fd: %d, com_id: %d", client->fd, com_id);

            comm_dict_delete(RSU_server.broker->client_dict, client->com_id);
            conn_free_client(client);
            return COM_IO_ERR;
        }
    }
    return COM_IO_OK;
}
int com_unlink(int com_id)
{
    client_t *client = comm_dict_find(RSU_server.broker->client_dict, com_id);
    if (client == NULL)
        return COM_IO_ERR;
    comm_dict_delete(RSU_server.broker->client_dict, client->com_id);
    conn_free_client(client);
    return COM_IO_OK;
}
