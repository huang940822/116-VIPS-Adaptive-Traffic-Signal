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
#include "config.h"
#include "log.h"
#include "typedefine.h"
#include "vector.h"

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

void CMS_encryptAES(FILE *input, FILE *output, const unsigned char *key)
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
        read_shift = 0;
    }

    EVP_EncryptFinal_ex(ctx, output_buffer, &encrypted_length);
    fwrite(output_buffer, 1, encrypted_length, output);

    EVP_CIPHER_CTX_free(ctx);
}

int CMS_update_database(int imgID, char *imgName)
{
    char *filename[256] = {0};
    char buffer[120];
    int n;
    FILE *database = fopen(CMS_pic_database_path, "r");
    if (database == NULL) {
        log_file_write_with_errno("Error opening input file %s", CMS_pic_database_path);
        perror("Error opening input file");
        database = fopen(CMS_pic_database_path, "w");
        if (database == NULL) {
            perror("Error opening/creating input file");
            log_file_write_with_errno("Error opening/creating input file %s", CMS_pic_database_path);
            return -1;
        }
        log_file_write("creating input file %s", CMS_pic_database_path);
        fclose(database);
        goto WRITEDATABASE;
    }

    while (n = fread(buffer, 1, sizeof(buffer), database) > 0) {
        vector_t(char *) str_arr;
        vector_init(str_arr);
        read_string_arr_from_config_line(buffer, &str_arr, " ");
        if (vector_size(str_arr) == 2) {
            char *substr = vector_at(str_arr, 0);
            int int_val;
            if (sscanf(substr, "%d", &int_val) == 1) {
                if (filename[int_val] != NULL)
                    free(filename[int_val]);
                filename[int_val] = strdup(vector_at(str_arr, 1));
                if (filename[int_val] == NULL) {
                    set_memory_error();
                    log_file_write_fatal_error("cms filename : malloc");
                    perror("cms filename : malloc");
                    exit(errno);
                }
            }
        }
        vector_free(str_arr);
    }
    fclose(database);

WRITEDATABASE:
    if (filename[imgID] != NULL)
        free(filename[imgID]);
    filename[imgID] = strdup(imgName);
    if (filename[imgID] == NULL) {
        set_memory_error();
        log_file_write_fatal_error("cms filename : malloc");
        perror("cms filename : malloc");
        exit(errno);
    }

    int ret = -1;
    database = fopen(CMS_pic_database_path, "w");
    if (database == NULL) {
        log_file_write_with_errno("Error output input file %s", CMS_pic_database_path);
        perror("Error opening output file");
        for (int i = 0; i < 256; i++)
            if (filename[i])
                free(filename[i]);
        goto FREE_FILENAME;
    }

    for (int i = 0; i < 256; i++) {
        if (filename[i]) {
            int len = snprintf(buffer, sizeof(buffer), "%d %s\n", i, filename[i]);
            len = fwrite(buffer, 1, len, database);
            if (len < 0) {
                goto CLOSEDATABASE;
            }
        }
    }
    ret = 1;
CLOSEDATABASE:
    fclose(database);
FREE_FILENAME:
    for (int i = 0; i < 256; i++)
        if (filename[i])
            free(filename[i]);
    return ret;
}

int CMS_img_hash(FILE *input_file, char *hashcode)
{
    int n;
    unsigned char input_buffer[BUFFER_SIZE];
    unsigned char SHA256_hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX *sha_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha_ctx, EVP_sha256(), NULL);
    while ((n = fread(input_buffer, 1, BUFFER_SIZE, input_file)) > 0) {
        EVP_DigestUpdate(sha_ctx, input_buffer, n);
    }
    EVP_DigestFinal_ex(sha_ctx, SHA256_hash, NULL);
    EVP_MD_CTX_free(sha_ctx);

    memset(hashcode, 0, 16);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hashcode[i % 16] += SHA256_hash[i];
    }
    return 1;
}

// 上傳圖片交由 middleware 執行，application 只有變更顯示圖片的權力
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

    FILE *encrypted_file = fopen("./" CMS_encrypt_img, "w");
    if (encrypted_file == NULL) {
        log_file_write_with_errno("Error opening encrypted file ./" CMS_encrypt_img);
        perror("Error opening encrypted file");
        fclose(input_file);
        return -1;
    }

    CMS_encryptAES(input_file, encrypted_file, CMS_key);

    fclose(input_file);
    fclose(encrypted_file);

    // SCP 上傳圖片
    char scp_command[1024];
    char ip[INET_ADDRSTRLEN];
    int fail = 0;
    char filename[120];
    snprintf(filename, sizeof(filename), "%03d_%s", imgID, imgName);
    for (int i = 0; i < cms_num; i++) {
        inet_ntop(AF_INET, &(cms_addrs[i].addr.sin_addr), ip, INET_ADDRSTRLEN);
        snprintf(scp_command, sizeof(scp_command), "timeout 5 scp ./" CMS_encrypt_img " " CMS_scp_path "%s:~/CMS/%s", ip, filename);

        int result = system(scp_command);
        if (result == 0) {
            printf("File transferred successfully.\n");
            log_file_write_fatal_error("CMS error SCP file transferred successfully. cms id %d ip %s", imgID, ip);
        } else {
            log_file_write_fatal_error("CMS error SCP failed. cms id %d ip %s", imgID, ip);
            printf("SCP failed.\n");
            fail++;  // 上船十次失敗回報
            i--;
            if (fail > CMS_update_fail_time) {
                remove("./" CMS_encrypt_img);
                set_vms_error();
                return -1;
            }
        }
    }

    remove("./" CMS_encrypt_img);

    // 更新資料庫
    if (CMS_update_database(imgID, imgName) < 0)
        return -1;

    // 發送更新指令
    char buffer[100];
    int buffer_len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    addr.sin_port = htons(CMS_PORT);


    buffer_len = 1;
    buffer[buffer_len++] = 1;  // CMD
    buffer[buffer_len++] = 1;  // type
    strcpy(buffer + 3, filename);
    buffer_len += strlen(filename);

    buffer[0] = buffer_len;
    if (sendto(cms_sockfd, buffer, buffer_len, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("sendto failed");
        close(cms_sockfd);
        exit(EXIT_FAILURE);
    }

    return 1;
}

int CMS_compare_hash(int cmsID, int imgID, uint8_t *imghash, uint8_t **hash_code)
{
    if (*hash_code != NULL && memcmp(*hash_code, imghash, 16) == 0) {
        return 1;
    }
    if (*hash_code)
        free(*hash_code);
    *hash_code = NULL;

    FILE *database = fopen(CMS_pic_database_path, "r");
    if (database == NULL) {
        log_file_write_with_errno("Error opening database file %s", CMS_pic_database_path);
        perror("Error opening database file");
        return -1;
    }

    char buffer[120];
    int n, flag = 0;
    while (n = fread(buffer, 1, sizeof(buffer), database) > 0) {
        vector_t(char *) str_arr;
        vector_init(str_arr);
        read_string_arr_from_config_line(buffer, &str_arr, " ");
        if (vector_size(str_arr) == 2) {
            char *substr = vector_at(str_arr, 0);
            int int_val;
            if (sscanf(substr, "%d", &int_val) == 1) {
                if (int_val == imgID) {
                    strncpy(buffer, vector_at(str_arr, 1), sizeof(buffer));
                    vector_free(str_arr);
                    flag = 1;
                    break;
                }
            }
        }
        vector_free(str_arr);
    }
    fclose(database);
    if (flag == 0) {
        log_file_write_with_errno("hash can't find imgID %s", imgID);
        printf("hash can't find imgID %d", imgID);
        return -1;
    }

    char path[100];
    strcpy(path, CMS_pic_path);
    strcat(path, buffer);
    FILE *input_file = fopen(path, "rb");
    if (input_file == NULL) {
        log_file_write_with_errno("Error opening input file %s", path);
        perror("Error opening input file");
        return -1;
    }

    FILE *encrypted_file = fopen("./" CMS_encrypt_img, "w");
    if (encrypted_file == NULL) {
        log_file_write_with_errno("Error opening encrypted file %s", CMS_encrypt_img);
        perror("Error opening encrypted file");
        fclose(input_file);
        return -1;
    }

    CMS_encryptAES(input_file, encrypted_file, CMS_key);
    fclose(input_file);
    fclose(encrypted_file);

    input_file = fopen("./" CMS_encrypt_img, "r");
    if (input_file == NULL) {
        log_file_write_with_errno("Error opening input file %s", "./" CMS_encrypt_img);
        perror("Error opening input file");
        return -1;
    }

    if (*hash_code == NULL) {
        uint8_t *hash_ptr = NULL;
        Malloc(hash_ptr, 16, "cms hash code");
        if (CMS_img_hash(input_file, hash_ptr) < 0) {
            free(hash_ptr);
            log_file_write_fatal_error("cms error cmsID %d imgID %d img_hash read error.", cmsID, imgID);
            set_vms_error();
        } else {
            *hash_code = hash_ptr;
        }
    }
    int ret = -1;
    if (memcmp(*hash_code, imghash, 16) != 0) {
        if (*hash_code)
            free(*hash_code);
        *hash_code = NULL;
        uint8_t *hash_ptr = NULL;
        Malloc(hash_ptr, 16, "cms hash code");
        fseek(input_file, 0, SEEK_SET);
        if (CMS_img_hash(input_file, hash_ptr) < 0) {
            free(hash_ptr);
            log_file_write_fatal_error("cms error cmsID %d imgID %d img_hash read error.", cmsID, imgID);
            set_vms_error();
        } else {
            if (memcmp(*hash_code, imghash + 6, 16) != 0) {
                log_file_write_fatal_error("cms error cmsID %d imgID %d hash not match.", cmsID, imgID);
                for (int i = 0; i < 16; i++) {
                    printf("%02x ", hash_ptr[i]);
                }
                printf("\n");
                for (int i = 0; i < 16; i++) {
                    printf("%d ", imghash[i]);
                }
                printf("\n");
                set_vms_error();
                *hash_code = NULL;
                free(hash_ptr);
            } else {
                ret = 1;
            }
        }
    } else {
        ret = 1;
    }

    remove("./" CMS_encrypt_img);
    return ret;
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
    uint8_t buffer[BUFFER_SIZE];
    uint8_t recvflags[CMS_NUM_MAX];
    uint8_t *img_hash[256];
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
                int cmsID = buffer[3];
                int status = buffer[4];
                int imgID = buffer[5];

                switch (cmd) {
                case 1: {
                    if (status == 1) {
                        log_file_write_fatal_error("cms error cmsID %d imgID %d No pic.", cmsID, imgID);
                        set_vms_error();
                    } else if (status == 2) {
                        log_file_write_fatal_error("cms error cmsID %d imgID %d pic name error.", cmsID, imgID);
                        set_vms_error();
                    } else {
                        if (CMS_compare_hash(cmsID, imgID, buffer + 6, &img_hash[imgID]) < 0) {
                            printf("------not match\n");
                        } else {
                            printf("------match\n");
                        }
                    }
                } break;
                case 2: {
                    if (status == 1) {
                        log_file_write_fatal_error("cms error cmsID %d imgID %d No pic.", cmsID, imgID);
                        set_vms_error();
                    } else if (status == 2) {
                        log_file_write_fatal_error("cms error cmsID %d imgID %d pic name error.", cmsID, imgID);
                        set_vms_error();
                    } else {
                        if (CMS_compare_hash(cmsID, imgID, buffer + 6, &img_hash[imgID]) < 0) {
                            printf("------not match\n");
                        } else {
                            printf("------match\n");
                        }
                    }
                } break;
                }

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
    sleep(5);
    // CMS_update_img(14, "BkF3.gif");
    CMS_update_img(14, "4456.gif");
}
