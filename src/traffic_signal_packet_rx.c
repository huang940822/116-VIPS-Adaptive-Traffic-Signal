#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "error_status.h"
#include "log.h"
#include "traffic_signal_packet_rx.h"
#include "traffic_signal_status_updating.h"
#include "typedefine.h"

int serial_port_fd;  // serial port

int16_t ack_seq = 0;

void traffic_signal_packet_init(traffic_signal_packet_t *packet)
{
    packet->DLE_1 = DLE_VAL;
}

void traffic_signal_packet_clean(traffic_signal_packet_t *packet)
{
    packet->DLE_2 = 0;
    packet->ETX = 0;
}

void read_header(int fd, traffic_signal_packet_t *packet)
{
    traffic_signal_packet_clean(packet);
    int ret = read(fd, &packet->SEQ, 5);
    if (ret == -1) {
        log_file_write_fatal_error("read_header: read");
    }
}

void read_trailer(int fd, traffic_signal_packet_t *packet)
{
    int ret = read(fd, &packet->DLE_2, 2);
    if (ret == -1) {
        log_file_write_fatal_error("read_trailer: read");
    }
}

int check_sum(traffic_signal_packet_t *packet, int info_len)
{
    uint8_t CKS = 0;
    uint8_t header_CKS = 0;
    uint8_t info_CKS = 0;
    uint8_t *tmp = NULL;

    tmp = &packet->DLE_1;
    for (int i = 0; i < 9; i++) {
        header_CKS ^= *tmp;
        tmp++;
    }

    tmp = packet->INFO;
    for (int i = 0; i < info_len; i++) {
        info_CKS ^= *tmp;
        tmp++;
    }
    CKS = header_CKS ^ info_CKS;
    return CKS;
}

int error_length(traffic_signal_packet_t *packet)
{
    if (packet->DLE_2 == DLE_VAL && packet->ETX == ETX_VAL)
        return 0;
    return 1;
}

int error_cks(traffic_signal_packet_t *packet, int CKS)
{
    if (packet->CKS == CKS)
        return 0;
    // printf("packet->cks: %x, cks: %x\n", packet->CKS, CKS);
    return 1;
}

/*int error_escape(traffic_signal_packet_t *packet, int info_len)
{
    uint8_t *tmp = packet->INFO;
    for (int i = 0; i < info_len; i++) {

        tmp++;
    }
}*/

void send_ack(int fd, traffic_signal_packet_t *packet)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: ACK\n");

    uint8_t output_byte[ACK_LEN1_VAL];
    uint8_t CKS = 0;

    packet->TYPE = ACK_VAL;
    // packet->ADDR[0] = ADDR0_VAL;
    // packet->ADDR[1] = ADDR1_VAL;
    packet->LEN[0] = ACK_LEN0_VAL;
    packet->LEN[1] = ACK_LEN1_VAL;
    traffic_signal_packet_clean(packet);

    CKS = check_sum(packet, ACK_INFO_LEN);

    memcpy(output_byte, &packet->DLE_1, 7);
    output_byte[7] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < ACK_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }

    int ret = write(serial_port_fd, output_byte, ACK_LEN1_VAL);
    if (ret == -1) {
        log_file_write_fatal_error("send_ack: write");
    }
    // tcdrain(serial_port_fd);

    return;
}

void send_nak(int fd, traffic_signal_packet_t *packet, uint8_t ERR)
{
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet tx: NAK\n");

    uint8_t output_byte[NAK_LEN1_VAL];
    uint8_t CKS = 0;
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "(%d)", ERR);

    packet->TYPE = NAK_VAL;
    packet->ADDR[0] = ADDR0_VAL;
    packet->ADDR[1] = ADDR1_VAL;
    packet->LEN[0] = NAK_LEN0_VAL;
    packet->LEN[1] = NAK_LEN1_VAL;
    traffic_signal_packet_clean(packet);
    memcpy(packet->INFO, &ERR, 1);

    CKS = check_sum(packet, NAK_INFO_LEN);

    memcpy(output_byte, &packet->DLE_1, 7);
    output_byte[7] = ERR;
    output_byte[8] = CKS;

    if (config.log_signal_packet_tx) {
        for (int i = 0; i < NAK_LEN1_VAL; i++) {
            snprintf(log_content + strlen(log_content),
                     LOG_CONTENT_LEN - strlen(log_content), "%x ",
                     output_byte[i]);
        }
        log_file_write(log_content);
    }

    int ret = write(serial_port_fd, output_byte, NAK_LEN1_VAL);
    if (ret == -1) {
        log_file_write_fatal_error("send_nak: write");
    }
    // tcdrain(serial_port_fd);

    return;
}

void print_packet(traffic_signal_packet_t *packet)
{
    if (config.log_signal_packet_rx == 0) {
        return;
    }
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "signal packet rx: ");

    switch (packet->TYPE) {
    case STX_VAL:
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%s\n", "INFO");
        break;
    case ACK_VAL:
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%s\n", "ACK");
        break;
    case NAK_VAL:
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%s\n", "NAK");
        break;

    default:
        break;
    }

    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->DLE_1);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->TYPE);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->SEQ);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->ADDR[0]);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->ADDR[1]);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->LEN[0]);
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->LEN[1]);
    for (int i = 0; i < ((packet->LEN[0] << 8 | packet->LEN[1]) - HEADER_LEN);
         i++) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->INFO[i]);
    }
    if (packet->TYPE == STX_VAL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->DLE_2);
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->ETX);
    }
    if (packet->TYPE == NAK_VAL) {
        snprintf(log_content + strlen(log_content),
                 LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->INFO[0]);
    }
    snprintf(log_content + strlen(log_content),
             LOG_CONTENT_LEN - strlen(log_content), "%x ", packet->CKS);
    log_file_write(log_content);
    return;
}

void recv_info(int fd, traffic_signal_packet_t *packet)
{
    // printf("recv_info\n");
    read_header(fd, packet);

    uint16_t packet_len = 0;
    uint16_t info_len = 0;
    uint8_t CKS = 0;

    packet_len = (uint16_t) packet->LEN[0] << 8 | packet->LEN[1];
    info_len = packet_len - HEADER_LEN;


    // 處理info欄位
    int ret = 0;
    uint16_t info_index = 0;
    uint16_t info_less = 0;
    while (info_index < info_len) {
        info_less = info_len - info_index;

        //為何要特別處理小於vmin_len的狀況？
        if (info_less < VMIN_LEN) {
            ret = read(fd, &packet->INFO[info_index], info_less);
            if (ret == -1) {
                log_file_write_fatal_error("recv_info: read");
            }
            info_index += info_less;
        } else {
            ret = read(fd, &packet->INFO[info_index], VMIN_LEN);
            if (ret == -1) {
                log_file_write_fatal_error("recv_info: read");
            }
            info_index += VMIN_LEN;
        }
    }

    read_trailer(fd, packet);

    // printf("info_len: %d\n", info_len);
    // print_packet(packet);
    //長度錯誤？
    if (error_length(packet) == 1) {
        print_packet(packet);
        send_nak(fd, packet, ERR_LENGTH);
        return;
    }

    // cks錯誤
    ret = read(fd, &packet->CKS, 1);
    if (ret == -1) {
        log_file_write_fatal_error("recv_info: read");
    }
    print_packet(packet);
    CKS = check_sum(packet, info_len);
    if (error_cks(packet, CKS) == 1) {
        send_nak(fd, packet, ERR_CKS);
        return;
    }

    send_ack(fd, packet);


    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0xCC) {
        packet_5FCC(packet);
    }
    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0xC8) {
        packet_5FC8(packet);
    }
    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0xC5) {
        packet_5FC5(packet);
    }
    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0xC4) {
        packet_5FC4(packet);
    }
    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0x0C) {
        packet_5F0C(packet);
    }
    if (packet->INFO[0] == 0x5F && packet->INFO[1] == 0xC3) {
        packet_5FC3(packet);
    }
    if (packet->INFO[0] == 0x0F && packet->INFO[1] == 0xC2) {
        packet_0FC2(packet);
    }
    if (packet->INFO[0] == 0x0F && packet->INFO[1] == 0x04) {
        // printf("tc status report\r\n");
        packet_0F04(packet);
    }
    if (packet->INFO[0] == 0x0F && packet->INFO[1] == 0x80) {
        // printf("tc status report\r\n");
        // packet_0F04(packet);
        printf("correct packet sent\r\n");
        printf("%02X  %02X\r\n", packet->INFO[2], packet->INFO[3]);
    }
    if (packet->INFO[0] == 0x0F && packet->INFO[1] == 0x81) {
        // printf("tc status report\r\n");
        // packet_0F04(packet);
        // log_file_write("error packet sent\r\n");
        printf("%02X  %02X %d\r\n", packet->INFO[2], packet->INFO[3],
               packet->INFO[4]);
    }
    if (packet->INFO[0] == 0x0F && packet->INFO[1] == 0xC3) {
        // printf("tc status report\r\n");
        // packet_0F04(packet);
        // log_file_write("error packet sent\r\n");
        if (packet->INFO[2] >= 0x6E) {  // 0x6e is 110年度
            if (config.signal_controller_manufacturer ==
                1) {  //若為山竚且年份大於110年度則改為修改過的山竚型號行為(for
                      //行人倒數秒數指令)
                config.signal_controller_manufacturer = 2;
                printf(
                    "new version of shan_zhu and should assign version as "
                    "shan_zhu_m\r\n");
                log_file_write(
                    "manufacturer is changed from shan_zhu to shan_zhu_m\r\n");
            }
        }
    }
    return;
}

int16_t recv_ack(int fd, traffic_signal_packet_t *packet)
{
    // printf("recv_ACK\n");
    read_header(fd, packet);

    uint8_t CKS = 0;

    int ret = read(fd, &packet->CKS, 1);
    if (ret == -1) {
        log_file_write_fatal_error("recv_ack: read");
    }
    print_packet(packet);
    CKS = check_sum(packet, ACK_INFO_LEN);
    if (error_cks(packet, CKS) == 1) {
        return -1;
    }
    // printf("recv ack\r\n");
    return packet->SEQ;
}

void recv_nak(int fd, traffic_signal_packet_t *packet)
{
    // printf("recv_NAK\n");
    int ret = 0;
    read_header(fd, packet);
    ret = read(fd, packet->INFO, 1);
    if (ret == -1) {
        log_file_write_fatal_error("recv_nak: read");
    }

    uint8_t CKS = 0;
    ret = read(fd, &packet->CKS, 1);
    if (ret == -1) {
        log_file_write_fatal_error("recv_nak: read");
    }
    print_packet(packet);
    CKS = check_sum(packet, NAK_INFO_LEN);
    if (error_cks(packet, CKS) == 1) {
        return;
    }
    printf("recv nack\r\n");
    return;
}

/*****************************************************************************
** Function:    set_serial_attribs
** Description: Setting the attributes of the serial port using termios
*structure.
**              8N1 Mode / Non Cannonical Mode.
** Parameter:   none
** Return:      none
** Reference:
*https://blog.xuite.net/uwlib_mud/twblog/108242774-Linux+RS-232+%E7%A8%8B%E5%BC%8F%E8%A8%AD%E8%A8%88
**				http://blog.gitdns.org/2016/10/20/uart-c/
******************************************************************************/
void set_serial_attribs()
{
    struct termios serial_port_settings; /* Create the structure */

    tcgetattr(serial_port_fd,
              &serial_port_settings); /* Get the current attributes of the
                                         Serial port */

    /* Setting the Baud rate */
    cfsetispeed(&serial_port_settings, BAUDRATE); /* Set Read  Speed as 9600 */
    cfsetospeed(&serial_port_settings, BAUDRATE); /* Set Write Speed as 9600 */

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
    if ((tcsetattr(serial_port_fd, TCSANOW, &serial_port_settings)) != 0) {
        log_file_write_fatal_error("error setting attributes of %s",
                                   SERIAL_PORT);
    } else {
        log_file_write("%s set attributes successfully", SERIAL_PORT);
    }
    sleep(2); /* required to make flush work, for some reason */
    tcflush(serial_port_fd,
            TCIOFLUSH); /* Discards old data in the rx buffer 		  */
}

void traffic_signal_port_init()
{
    serial_port_fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    /* ttyUSB0 is the FT232 based USB2SERIAL Converter   */
    /* O_RDWR   - Read/Write access to serial port       */
    /* O_NOCTTY - No terminal will control the process   */
    /* Open in blocking mode,read will wait              */

    /* Error Checking */
    if (serial_port_fd == -1) {
        log_file_write_fatal_error("error opening %s", SERIAL_PORT);
    } else {
        log_file_write("%s opened successfully", SERIAL_PORT);
    }
    set_serial_attribs();
}

/* traffic_signal_packet_thread */
void *traffic_signal_packet_rx_handler()
{
    if (serial_port_fd == -1)
        return NULL;

    uint8_t read_buffer[1]; /* Buffer to store the data received */
    uint8_t bytes_read = 0; /* Number of bytes read by the read() system call */
    bool escape_flag = 0;   // what for???

    traffic_signal_packet_t *packet =
        (traffic_signal_packet_t *) malloc(MAX_PACKET_LEN);
    if (packet == NULL) {
        set_memory_error();
        log_file_write_fatal_error("traffic_signal_packet_rx_handler: malloc");
        perror("traffic_signal_packet_rx_handler: malloc");
        exit(errno);
    } else {
        clear_memory_error();
        memset(packet, 0, MAX_PACKET_LEN);
    }
    traffic_signal_packet_init(packet);

    while (1) {
        bytes_read = read(serial_port_fd, &read_buffer, 1); /* Read the data */
        // printf("%x\n", read_buffer[0]);
        if (bytes_read == 0) {
            log_file_write_fatal_error(
                "traffic_signal_packet_rx_handler: read");
            perror("traffic_signal_packet_rx_handler: read");
            exit(errno);
        }
        /* 0xAA */
        if (read_buffer[0] == DLE_VAL && escape_flag == 0) {
            escape_flag = 1;
            continue;
        }

        if (escape_flag == 1) {
            /* 0xAA 0xBB */
            if (read_buffer[0] == STX_VAL) {
                packet->TYPE = STX_VAL;
                recv_info(serial_port_fd, packet);
            }
            /* 0xAA 0xDD */
            else if (read_buffer[0] == ACK_VAL) {
                packet->TYPE = ACK_VAL;
                ack_seq = recv_ack(serial_port_fd, packet);
                // printf("ack seq is %d\r\n",ack_seq);
            }
            /* 0xAA 0xEE */
            else if (read_buffer[0] == NAK_VAL) {
                packet->TYPE = NAK_VAL;
                recv_nak(serial_port_fd, packet);
            } else {
                escape_flag = 0;
            }
        }
        escape_flag = 0;
    }
    // printf("\n +----------------------------------+\n\n\n");
    close(serial_port_fd); /* Close the serial port */
}
