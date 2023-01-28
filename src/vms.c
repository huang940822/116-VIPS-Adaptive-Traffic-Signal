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

pthread_mutex_t sec_mutex = PTHREAD_MUTEX_INITIALIZER;
timer_t controller_polling_timer_id;
uint8_t controller_polling_value = 0;

int port_fd;

int program_carousel[CAROUSEL_NUM];
int carousel_time[CAROUSEL_NUM];
int current_program;
int remain_time;
int request_number; // 初始值為預設輪播，應用層用extern的方式來改，注意mutex
int service_number; // 初始值為預設輪播，表示為當前正在服務的對象


void carousel_update(){  // 雲端下了更新輪播，就要執行這個函數來更新config和輪播陣列
    // update config
    // update program_carousel[] and carousel_time[]
    // reset current_program to 0
}

void control_loop(){
    // 比較request_number有沒有比service_number小，有的話則服務該請求，
    // 並把service_number設為它
    // 如果request_number為-1則直接執行預設輪播
    printf("Hello!\n");
}

void vms_handler_init(){
    port_fd = open(VMS_SERIAL_PORT, O_RDWR | O_NOCTTY);

    if (port_fd == -1) {
        log_file_write_fatal_error("error opening %s", VMS_SERIAL_PORT);
    }
    else {
        log_file_write("%s opened successfully", VMS_SERIAL_PORT);
    }

    vms_set_serial_attribs();
}

void vms_set_serial_attribs()
{
    struct termios serial_port_settings; /* Create the structure */

    tcgetattr(port_fd, &serial_port_settings); /* Get the current attributes of the Serial port */

    /* Setting the Baud rate */
    cfsetispeed(&serial_port_settings, VMS_BAUDRATE); /* Set Read  Speed as 9600 */
    cfsetospeed(&serial_port_settings, VMS_BAUDRATE); /* Set Write Speed as 9600 */

    /* 8N1 Mode */
    serial_port_settings.c_cflag &=
        ~PARENB; /* Disables the Parity Enable bit(PARENB),So No Parity   */
    serial_port_settings.c_cflag &=
        ~CSTOPB; /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
    serial_port_settings.c_cflag &=
        ~CSIZE;                          /* Clears the mask for setting the data size             */
    serial_port_settings.c_cflag |= CS8; /* Set the data bits = 8 */

    serial_port_settings.c_cflag |=
        CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */

    serial_port_settings.c_iflag =
        IGNPAR; /* Ignore framing errors and parity errors */
    serial_port_settings.c_oflag &=
        ~OPOST; /* No Output Processing					 */
    serial_port_settings.c_lflag &=
        ~(ICANON | ECHO | ECHOE | ISIG); /* Non Cannonical mode */

    /* Setting Time outs */
    serial_port_settings.c_cc[VMIN] =
        VMIN_LEN;                         /* Read at least 20 characters */
    serial_port_settings.c_cc[VTIME] = 0; /* Wait indefinetly            */

    /* Set the attributes to the termios structure */
    if ((tcsetattr(port_fd, TCSANOW, &serial_port_settings)) != 0) {
        log_file_write_fatal_error("error setting attributes of %s",
                                   VMS_SERIAL_PORT);
    } else {
        log_file_write("%s set attributes successfully", VMS_SERIAL_PORT);
    }
    sleep(2); /* required to make flush work, for some reason */
    tcflush(port_fd,
            TCIOFLUSH); /* Discards old data in the rx buffer 		  */
}

void *vms_handler() {
    vms_handler_init();

    while(1) {
        control_loop();
        sleep(1);
    }
    close(port_fd);
}
