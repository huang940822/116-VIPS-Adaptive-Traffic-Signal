#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "timer_event.h"
#include "vms.h"
#include "traffic_signal_status_updating.h"
#include "config.h"
#include "log.h"

int port_fd;

void vms_handler_init(){
    
}