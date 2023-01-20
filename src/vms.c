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
#include "traffic_signal_packet_rx.h"
#include "config.h"
#include "log.h"

pthread_mutex_t sec_mutex = PTHREAD_MUTEX_INITIALIZER;
timer_t controller_polling_timer_id;
uint8_t controller_polling_value = 0;

int port_fd;

void control_loop(){
    printf("Hello!\n");
}

void vms_handler_init(){
    port_fd = open(VMS_SERIAL_PORT, O_RDWR | O_NOCTTY);
    set_serial_attribs(port_fd, VMS_BAUDRATE, VMS_SERIAL_PORT);
}
