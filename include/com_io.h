#ifndef COM_IO
#define COM_IO
#include "server.h"
#define COM_IO_OK 0
#define COM_IO_ERR -1
int com_send(int com_id, unsigned char *buf, size_t send_len);
int com_unlink(int com_id);
#endif