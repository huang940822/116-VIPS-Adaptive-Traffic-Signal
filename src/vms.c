#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "byte_processing.h"
#include "config.h"
#include "com_packet_processing.h"
#include "error_status.h"
#include "log.h"
#include "network.h"
#include "typedefine.h"
#include "timer_event.h"
#include "traffic_signal_status_updating.h"
#include "vms.h"

pthread_mutex_t VMS_request_priority_mutex = PTHREAD_MUTEX_INITIALIZER;

traffic_signal_status_t signal_status;
uint8_t rtm_phase[RTM_MAX] = {0};
char current_step[RTM_MAX];

int port_fd;

uint8_t request_priority; // 應用層用vms_request_start()的方式來改，初始值為255(預設輪播)
uint8_t app_id; // 表示為當前正在服務的對象，初始值為255(預設輪播)

uint8_t evsp_prog[RTM_MAX];   // 之後改成[246,247,248,249,0,0...]

// tx sequence format: (seq,p1,p2,p3,p4\n
// rx sequence format: (seq,reserve,programNo,location\n
char vms_packet_tx[VMS_PACKET_TX_LEN_MAX];
char vms_packet_rx[VMS_PACKET_RX_LEN_MAX];
char uint8_t_to_char[10];

int sequence_number;
int res;

uint8_t vms_respose_cnt[RTM_MAX];
int readCnt;

void vms_request_start(uint8_t id, uint8_t priority)
{
    pthread_mutex_lock(&VMS_request_priority_mutex);
    if (priority < request_priority) {
        request_priority = priority;
        app_id = id;
    }
    pthread_mutex_unlock(&VMS_request_priority_mutex);

}

void vms_request_end(uint8_t id)
{
    pthread_mutex_lock(&VMS_request_priority_mutex);
    // 只有自己能關掉自己的服務，避免其他應用在 timeout 的時候把別人的 VMS service 關起來。 
    if (app_id == id) {
        request_priority = CAROUSEL_NUM;
        app_id = CAROUSEL_NUM;
    }
    pthread_mutex_unlock(&VMS_request_priority_mutex);
}


int carousel_update(uint8_t VMS_ID, uint8_t Program_Type, uint8_t Program_ID)  // 雲端下了更新輪播，就要執行這個函數來更新輪播陣列
{   
    // 有空改 ENUM
    if (VMS_ID > 7) {
        return -1;
    }else if (Program_Type != 0 || Program_Type != 1) {
        return -2;
    }else if (Program_ID == 0) {
        return -3;
    }

    if (Program_Type == 0) {  // Green
        vms_config.program_ids_green[VMS_ID] = Program_ID;
        log_file_write("program_ids_green[%d] change to %d", VMS_ID, Program_ID);
        printf("program_ids_green[%d] change to %d", VMS_ID, Program_ID);
    }else if (Program_Type == 1) {    // Not Green
        vms_config.program_ids_not_green[VMS_ID] = Program_ID;
        log_file_write("program_ids_not_green[%d] change to %d", VMS_ID, Program_ID);
        printf("program_ids_not_green[%d] change to %d", VMS_ID, Program_ID);
    }
    return 0;
}

void VMS_report_programs_id(uint8_t cmd) 
{
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("VMS_report_programs_id: malloc");
        perror("VMS_report_programs_id: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(cmd, &write_buf);
    // Program IDs(Green)
    for (int i = 0; i < RTM_MAX; ++i) {
        write_uint8_t(vms_config.program_ids_green[i], &write_buf);
    }
    // Program IDs(Not Green)
    for (int i = 0; i < RTM_MAX; ++i) {
        write_uint8_t(vms_config.program_ids_not_green[i], &write_buf);
    }

    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);
    return;
}

void VMS_report_program_name(uint8_t cmd, uint8_t program_id)
{
    FILE *fp;
    int program_id = 255;
    char *pos;
    char line[256]; // 用於保存每一行的內容
    char search_str[8]; // 用於保存要查找的字符串
    char filename[100];

    memset(filename, 0, sizeof(filename));
    snprintf(search_str, sizeof(search_str), "%d ", program_id); // 生成要查找的字符串

    fp = fopen(VMS_pic_path, "r+");
    
    if (fp == NULL) {
        log_file_write_fatal_error("VMS_report_programs_name: open program_id.txt failed");
        return;
    }
    int flag = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, search_str, strlen(search_str)) == 0) {
            flag = 1;
            pos = strchr(line, ' ') + 1;
            if (pos != NULL) {
                strcpy(filename, pos);
            }
            break;
        }

    }

    if (flag == 0) {
        strcat(filename, "None\n");
    }

    fclose(fp);

    // 回傳給雲端
    msg_buf_t write_buf;
    write_buf.index = 0;
    write_buf.content = (unsigned char *) malloc(R2C_SPECIFIC_FIELD_MAX_LEN);
    if (write_buf.content == NULL) {
        set_memory_error();
        log_file_write_fatal_error("VMS_report_programs_name: malloc");
        perror("VMS_report_programs_name: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(write_buf.content, 0, R2C_SPECIFIC_FIELD_MAX_LEN);
    }

    // cmd
    write_uint8_t(cmd, &write_buf);
    // Program ID
    write_uint8_t(program_id, &write_buf);
    // Program Name, strlen(filename)-1 把換行字元刪掉
    filename[strlen(filename) - 1] = 0;
    write_char(filename, &write_buf, strlen(filename), PROGRAM_NAME_LEN);

    cloud_packet_tx(write_buf.index, TSP_ID, write_buf.content);
    free(write_buf.content);
    return;
}

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
    sequence_number = (sequence_number + 1) % 256;
    if (sequence_number == 0) {
        sequence_number ++;
    }

    // select
    /*while (1) {
        res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
        printf("Hello %d\n", res);
        
        if(res <= 0){
            break;
        }
        printf("vms_packet_rx: %s\n", vms_packet_rx);
    }*/

    get_traffic_signal_status(&signal_status);
    phase_rtm_connect();
    // printf("PhaseOrder %02x SubPhaseID %d StepID %d StepSec %d\n", signal_status.PhaseOrder, signal_status.SubPhaseID, signal_status.StepID, signal_status.StepSec);

    memset(vms_packet_rx, 0, sizeof(vms_packet_tx));

    strcpy(vms_packet_tx, VMS_PACKET_BEGIN);
    sprintf(uint8_t_to_char, "%d", sequence_number);
    strcat(vms_packet_tx, uint8_t_to_char);

    switch (app_id) {
        case EVSP_ID: // EVSP
        {
            for(int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
                sprintf(uint8_t_to_char, "%d", evsp_prog[i]);
                strcat(vms_packet_tx, VMS_PACKET_COMMA);
                strcat(vms_packet_tx, uint8_t_to_char);
            }

        }break;
        case CAROUSEL_NUM:
        {
            uint8_t current_phase = 1 << (signal_status.SubPhaseID - 1);
            for(int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
                if ((rtm_phase[i] & current_phase) > 0 && signal_status.StepID <= 3) {  // Green
                    current_step[i] = 'G';
                }
                else {  // Not Green
                    current_step[i] = 'R';
                }
            }

            for(int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
                if (current_step[i] == 'G') {
                    sprintf(uint8_t_to_char, "%d", vms_config.program_ids_green[i]);
                }
                else {
                    sprintf(uint8_t_to_char, "%d", vms_config.program_ids_not_green[i]);
                }
                strcat(vms_packet_tx, VMS_PACKET_COMMA);
                strcat(vms_packet_tx, uint8_t_to_char);
            }
        }break;
        default:
        {
            log_file_write("Useless vms app_id: %d", app_id);
        }break;
    }

    strcat(vms_packet_tx, VMS_PACKET_END);
    res = write(port_fd, vms_packet_tx, strlen(vms_packet_tx));
    if (res > 0) {
        log_file_write("vms_packet_tx: %s", vms_packet_tx);
        printf("vms_packet_tx: %s", vms_packet_tx);
    }
    sleep(1);
    res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
    // res == -1 case(EAGAIN)
    if (res < 0) {
        //處理timeout
        printf("RS232: EAGAIN\n");
    }else if (res > 0) {
        log_file_write("vms_packet_rx: %s", vms_packet_rx);
        printf("%s\n", vms_packet_rx);
    }

    readCnt++;
    
    for (int i = 0; i < strlen(vms_packet_rx)-1; i++) {
        if (vms_packet_rx[i+1] == '\n') {
            // 49 是因為 VMS 編號是從1開始 所以多減一
            vms_respose_cnt[vms_packet_rx[i]-49]++;
        }
    }

    printf("vms_respose_cnt:");
    for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
        printf("%d ", vms_respose_cnt[i]);
    }
    printf("\n");
    // 每傳送十次檢查一次有沒有VMS已經超過十秒沒有回應，有的話判定 VMS 異常
    // 因為有一塊板子被廠商拿走了，所以這段程式碼會一直觸發異常
    if (readCnt == VMS_ERROR_THRESHOLD) {
        int errorFlag = 0;
        for (int i = 0; i < RTM_MAX && i < signal_status.SignalCount; i++) {
            if (vms_respose_cnt[i] == 0) {
                errorFlag = 1;
                log_file_write_fatal_error("VMS_id : %d no respose", i+1);
            }
        }
        if (errorFlag == 1) {
            set_vms_error();
            log_file_write_fatal_error("VMS : respose error");
        }else {
            clear_vms_error();
        }

        readCnt = 0;
        memset(vms_respose_cnt, 0, sizeof(vms_respose_cnt));
    }
    
}

void vms_handler_init()
{   
    srand(time(NULL));
    sequence_number = ( rand() % CAROUSEL_NUM ) + 1 ;
    readCnt = 0;
    memset(vms_respose_cnt, 0, sizeof(vms_respose_cnt));
    request_priority = CAROUSEL_NUM;
    app_id = CAROUSEL_NUM;

    port_fd = open(VMS_SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (port_fd == -1) {
        log_file_write_fatal_error("error opening %s", VMS_SERIAL_PORT);
    }else {
        log_file_write("%s opened successfully", VMS_SERIAL_PORT);
    }

    vms_set_serial_attribs();

    res = net_non_block("", port_fd);

    // 需要做一次送編號全255的當作初始化，才不會IPC當機恢復之後因為 VMS timeout 所以沒辦法正常播放節目
    // 因為有一塊板子的wifi壞了，暫時沒辦法全部上傳黑色節目到編號255
    memset(vms_packet_rx, 0, sizeof(vms_packet_rx));
    memset(vms_packet_tx, 0, sizeof(vms_packet_tx));
    strcat(vms_packet_tx, VMS_PACKET_BEGIN);
    sprintf(uint8_t_to_char, "%d", sequence_number);
    strcat(vms_packet_tx, uint8_t_to_char);
    strcat(vms_packet_tx, ",255,255,255,255\n");
    res = write(port_fd, vms_packet_tx, strlen(vms_packet_tx));
    if (res > 0) {
        log_file_write("vms_packet_tx: %s", vms_packet_tx);
        printf("vms_packet_tx: %s", vms_packet_tx);
    }
    sleep(1);
    res = read(port_fd, vms_packet_rx, VMS_PACKET_RX_LEN_MAX);
    if (res < 0) {
        //處理異常
        printf("RS232: EAGAIN\n");
    }else if (res > 0) {
        log_file_write("vms_packet_rx: %s", vms_packet_rx);
        printf("%s\n", vms_packet_rx);
    }
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
