#ifndef EXTERNAL_APP_PROXY_H
#define EXTERNAL_APP_PROXY_H

enum ea_err_define_enum{
    EA_ERR_OK = 0,
    EA_ERR_SOCKET_CREATE,
    EA_ERR_SOCKET_CONNECT,
    EA_ERR_SOCKET_READ,
    EA_ERR_SOCKET_WRITE,
    EA_ERR_SOCKET_CLOSE,
    EA_ERR_API_UNDEFINDED,
    EA_ERR_BAD_PARAMETER,
    EA_ERR_J2735_MSG_ENCODE,
    EA_ERR_MEMORY_LIB,
    EA_ERR_COM_IO,

    /* this tag should always be at the last*/
    NUM_OF_EA_ERR_DEF,
};

void* external_app_proxy_handler();

#endif  /* EXTERNAL_APP_PROXY_H */