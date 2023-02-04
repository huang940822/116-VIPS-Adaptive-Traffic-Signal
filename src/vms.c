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
// timer_t controller_polling_timer_id;
// uint8_t controller_polling_value = 0;

traffic_signal_status_t signal_status;
// uint16_t rtm_sec[RTM_MAX];
uint8_t rtm_phase[RTM_MAX] = {0};

int port_fd;

// int program_carousel[CAROUSEL_NUM];
// int carousel_time[CAROUSEL_NUM];
// int current_program;
// int remain_time;
int request_priority; // 初始值為預設輪播，應用層用vms_request_start()的方式來改，注意mutex
int app_id; // 初始值為預設輪播，表示為當前正在服務的對象
int sequence_number;

int evsp_prog[RTM_MAX];   // 之後改成[246,247,248,249,0,0...],[2,3,4,1,0,0,...]...
char current_step[RTM_MAX];

// int atm[8]=[phase0,....,phase7]

void evsp_vms_service()
{

}

void vms_request_start(uint8_t id, uint8_t priority)
{
    // mutex
    request_priority = priority;
    //for loop
    //evsp_prog[4]
}

void vms_request_end(uint8_t id, uint8_t priority)
{

}

/*
void carousel_update(){  // 雲端下了更新輪播，就要執行這個函數來更新輪播陣列
    // update program_carousel[] and carousel_time[]
    // reset current_program to 0
}*/


void phase_rtm_connect()
{
    traffic_signal_status_t signal_status;
    get_traffic_signal_status(&signal_status);

    memset(rtm_phase, 0, sizeof(rtm_phase));
    for (int i = 0; i < signal_status.SubPhaseCount; i++) {
        for(int j = 0; j < signal_status.SignalCount && i < RTM_MAX; j++) {
            if ((signal_status.phaseorder_plan[i][j].SignalStatus & 0b00111100) > 0) {
                rtm_phase[j] |= (1 << i);
            }
        }
    }
}

void control_loop()
{
    // 比較request_number有沒有比service_number小，有的話則服務該請求，
    // 並把service_number設為它
    // 如果request_number為-1則直接執行預設輪播

    get_traffic_signal_status(&signal_status);
    phase_rtm_connect();
    // printf("PhaseOrder %02x SubPhaseID %d StepID %d StepSec %d\n", signal_status.PhaseOrder, signal_status.SubPhaseID, signal_status.StepID, signal_status.StepSec);
    uint8_t current_phase = 1 << (signal_status.SubPhaseID - 1);
    for(int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
        if ((rtm_phase[i] & current_phase) > 0 && signal_status.StepID <= 3) {  // Green
            current_step[i] = 'G';
        }
        else {  // Not Green
            current_step[i] = 'R';
        }
    }

    sequence_number = (sequence_number + 1) % 256;
    if (sequence_number == 0) {
        sequence_number ++;
    }

    /*for(int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
    }*/

    // printf("\n");
    //switch case(service_number)
        //1:evsp_vms_sevice();
}

void vms_handler_init()
{
    sequence_number = 1;
    port_fd = open(VMS_SERIAL_PORT, O_RDWR | O_NOCTTY);

    if (port_fd == -1) {
        log_file_write_fatal_error("error opening %s", VMS_SERIAL_PORT);
    }
    else {
        log_file_write("%s opened successfully", VMS_SERIAL_PORT);
    }

    vms_set_serial_attribs();
    
    printf("%d\n", vms_config.vms_active);
    for (int i = 0; i < RTM_MAX; ++i) {
        printf("%d ", vms_config.program_ids_green[i]);
    }
    printf("\n");
    for (int i = 0; i < RTM_MAX; ++i) {
        printf("%d ", vms_config.program_ids_not_green[i]);
    }
    printf("\n");
    // 需要做一次送編號全0的當作初始化，才不會IPC當機恢復之後因為 VMS timeout 所以沒辦法正常播放節目
    /*
    // 施工
    */
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

void *vms_handler()
{
    if (vms_config.vms_active == 0) {
        return NULL;
    }

    vms_handler_init();

    while(1) {
        control_loop();
        sleep(1);
    }

    close(port_fd);
}

// https://ncku365-my.sharepoint.com/:p:/g/personal/p76101160_ncku_edu_tw/EdZ5RSBI6kxKgc_KwgRGn-YBjgrhCRPNddsJdz9qVu0ZsQ?rtime=RSZUgDsE20g
