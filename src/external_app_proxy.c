#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "eap_typedefine.h"
#include "eap_inner_implementation.h"
#include "external_app_proxy.h"

void* external_app_proxy_handler()
{
    for(;;){ 
    
    }
    log_file_write_fatal_error("external_app_proxy_handler thread exit");
}
