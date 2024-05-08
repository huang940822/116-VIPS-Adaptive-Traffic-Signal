#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "cms.h"
#include "log.h"
#include "typedefine.h"

#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 1
#define CMS_NUM_MAX 8

const char CMS_header[] = {'m', '5', 'm', 'm', '4', 'm'};
const unsigned char CMS_key[16] = {'K', 'E', 'Y', 'K', 'E', 'Y', 'K', 'E', 'Y', 'K', 'E', 'Y', 'K', 'E', 'Y', 'K'};

int cms_sockfd;
pthread_mutex_t cms_prog_mutex = PTHREAD_MUTEX_INITIALIZER;
uint8_t cms_prog[CMS_NUM_MAX] = {14, 0, 0, 0, 0, 0, 0, 0};

int cms_num = 1;

struct CMS_elememt {
    struct sockaddr_in addr;
};
struct CMS_elememt cms_addrs[CMS_NUM_MAX] = {0};

void encryptAES(FILE *input, FILE *output, const unsigned char *key)
{
    EVP_CIPHER_CTX *ctx;
    ctx = EVP_CIPHER_CTX_new();

    EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);

    unsigned char input_buffer[BUFFER_SIZE];
    unsigned char output_buffer[BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int bytes_read, read_shift = sizeof(CMS_header);
    int encrypted_length;
    memcpy(input_buffer, CMS_header, sizeof(CMS_header));

    while ((bytes_read = fread(input_buffer + read_shift, 1, BUFFER_SIZE - read_shift, input)) > 0) {
        EVP_EncryptUpdate(ctx, output_buffer, &encrypted_length, input_buffer, bytes_read + read_shift);
        fwrite(output_buffer, 1, encrypted_length, output);
        printf("encrypted_length %d\n", encrypted_length);
        read_shift = 0;
    }

    EVP_EncryptFinal_ex(ctx, output_buffer, &encrypted_length);
    fwrite(output_buffer, 1, encrypted_length, output);

    EVP_CIPHER_CTX_free(ctx);
}

int CMS_update_img(int imgID, char *imgName)
{
    // 檢查資料架是否存在
    struct stat stat_buffer;
    if (stat(CMS_pic_path, &stat_buffer) != 0 || !S_ISDIR(stat_buffer.st_mode)) {
        printf(CMS_pic_path "does not exist.\n");
        log_file_write_fatal_error(CMS_pic_path "does not exist.");
        return -1;
    }

    char path[100];
    strcpy(path, CMS_pic_path);
    strcat(path, imgName);
    FILE *input_file = fopen(path, "rb");
    if (input_file == NULL) {
        log_file_write_with_errno("Error opening input file %s", path);
        perror("Error opening input file");
        return -1;
    }

    FILE *encrypted_file = fopen(CMS_pic_path CMS_encrypt_img, "w");
    if (encrypted_file == NULL) {
        log_file_write_with_errno("Error opening encrypted file " CMS_pic_path CMS_encrypt_img);
        perror("Error opening encrypted file");
        fclose(input_file);
        return -1;
    }

    encryptAES(input_file, encrypted_file, CMS_key);

    fclose(input_file);
    fclose(encrypted_file);

    char scp_command[1024];
    snprintf(scp_command, sizeof(scp_command), "timeout 5 scp %s %s/%03d_%s", CMS_pic_path CMS_encrypt_img, CMS_scp_path, imgID, imgName);

    int result = system(scp_command);
    if (result == 0) {
        printf("File transferred successfully.\n");
    } else {
        printf("SCP failed.\n");
    }

    remove(CMS_pic_path CMS_encrypt_img);
}

int CMS_update_client_addr(int CMSid, struct sockaddr_in *client_addr)
{
    if (memcmp(client_addr, &cms_addrs[CMSid - 1].addr, sizeof(struct sockaddr_in)) != 0) {
        char ip[INET_ADDRSTRLEN];
        int port;

        inet_ntop(AF_INET, &(client_addr->sin_addr), ip, INET_ADDRSTRLEN);

        port = ntohs(client_addr->sin_port);
        memcpy(&cms_addrs[CMSid - 1].addr, client_addr, sizeof(struct sockaddr_in));

        log_file_write("CMS ip update CMSID %d IP: %s, Port: %d", CMSid, ip, port);
        printf("IP: %s, Port: %d\n", ip, port);
    }
}

int CMS_recv_timeout(char *buffer, int buffer_len, struct sockaddr *client_addr, socklen_t *client_addr_len)
{
    int ret = -1;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(cms_sockfd, &readfds);

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    int activity = select(cms_sockfd + 1, &readfds, NULL, NULL, &timeout);

    if (activity == -1) {
        log_file_write_with_errno("cms select");
        exit(EXIT_FAILURE);
    } else if (activity == 0) {
        return ret;
    }

    if (FD_ISSET(cms_sockfd, &readfds)) {
        ret = recvfrom(cms_sockfd, buffer, buffer_len, MSG_WAITALL,
                       (struct sockaddr *) client_addr, client_addr_len);
    }
    return ret;
}

static void *CMS_handler()
{
    struct sockaddr_in addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(CMS_PORT);

    int buffer_len = 0;
    char buffer[BUFFER_SIZE];
    uint8_t recvflags[CMS_NUM_MAX];
    while (1) {
        buffer_len = 1;
        buffer[buffer_len++] = 2;  // CMD
        buffer[buffer_len++] = 1;  // type

        pthread_mutex_lock(&cms_prog_mutex);
        for (int i = 0; i < sizeof(cms_prog); i++) {
            buffer[buffer_len++] = i + 1;
            buffer[buffer_len++] = cms_prog[i];
        }
        pthread_mutex_unlock(&cms_prog_mutex);

        buffer[0] = buffer_len;
        if (sendto(cms_sockfd, buffer, buffer_len, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            perror("sendto failed");
            close(cms_sockfd);
            exit(EXIT_FAILURE);
        }
        int recv_flag = 0;
        memset(recvflags, 0, sizeof(recvflags));
        do {
            buffer_len = CMS_recv_timeout(buffer, BUFFER_SIZE, (struct sockaddr *) &client_addr, &client_addr_len);
            printf("buffer_len %d %d %d\n", buffer_len, buffer[0], buffer[3]);
            if (buffer_len < 0) {
                recv_flag++;
            } else {
                recv_flag = -1;
                if (buffer_len > 4 && buffer[0] == buffer_len && buffer[3] <= cms_num) {
                    recvflags[buffer[3]] = 1;
                    CMS_update_client_addr(buffer[3], &client_addr);
                }
                int cmd = buffer[2];
                for (int i = 0; i < cms_num; i++) {
                    if (recvflags[i] == 0) {
                        recv_flag = 0;
                        break;
                    }
                }
            }
        } while (recv_flag < 2 && recv_flag <= 0);
        if (recv_flag == -1) {
            clear_vms_error();
        } else {
            set_vms_error();
        }
        sleep(1);
    }
    close(cms_sockfd);
}

void CMS_handler_init()
{
    struct sockaddr_in addr;
    cms_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (cms_sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    char broadcast_ip[INET_ADDRSTRLEN];
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, CMS_INTERFACE_NAME) == 0) {
            struct sockaddr_in *addr = (struct sockaddr_in *) ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, broadcast_ip, INET_ADDRSTRLEN);
            break;
        }
    }

    freeifaddrs(ifaddr);

    if (strlen(broadcast_ip) == 0) {
        log_file_write_fatal_error(CMS_INTERFACE_NAME " interface not found or doesn't have an IP address\n");
        exit(EXIT_FAILURE);
    }

    // 設定允許廣播
    int broadcast_enable = 1;
    if (setsockopt(cms_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
        log_file_write_with_errno("cms setsockopt");
        close(cms_sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(broadcast_ip);
    addr.sin_port = htons(CMS_PORT);

    if (bind(cms_sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        log_file_write_with_errno("cms bind failed");
        close(cms_sockfd);
        exit(EXIT_FAILURE);
    }

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(cms_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_file_write_with_errno("cms setsockopt failed");
        close(cms_sockfd);
        exit(EXIT_FAILURE);
    }

    pthread_t cms_thread;
    int ret = pthread_create(&cms_thread, NULL, CMS_handler, NULL);
    if (ret != 0) {
        log_file_write_fatal_error("CMS_handler error creating cms_thread: %d", ret);
        perror("main: pthread_create");
        exit(errno);
    }

    CMS_update_img(14, "4456.gif");
}
