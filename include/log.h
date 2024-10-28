#ifndef LOG_H
#define LOG_H

#include "typedefine.h"
#include <time.h> 

#define LOG_DIR FILE_PATH "log/"

// #define LOG_FILE_NAME_FORMAT "%Y-%m-%d %H:%M"
// #define LOG_FILE_NAME_LEN 17

#define LOG_FILE_NAME_FORMAT "%Y-%m-%d %H"
#define LOG_FILE_NAME_LEN 14

#define ERR_MSG_SZ 128
#define LOG_CONTENT_LEN 2048

#define log_snprintf(log_content, ...)                                     \
    do {                                                                   \
        size_t _len = strlen(log_content);                                 \
        snprintf(log_content + _len, LOG_CONTENT_LEN - _len, __VA_ARGS__); \
    } while (0)

void log_file_init();
void log_file_name_update();
void log_file_write(const char *format, ...);
// void log_file_write_fatal_error(char *content);
void log_file_write_fatal_error(const char *format, ...);

#define ENABLE_FATAL_WITH_ERR_CODE_LOG 1
#define SWITCH_FATAL_WITH_ERR_CODE_LOG_TO_PRINT 1
int log_file_write_with_errno(const char *format, ...);

struct timespec get_timespec_diff(struct timespec bgn, struct timespec end);
uint32_t get_us_diff(struct timespec bgn, struct timespec end);
void record_current_timespec(struct timespec* now_p);
void print_timespec_to_stderr(struct timespec bgn, struct timespec end, char* msg);
void print_single_timespec_to_stdout(struct timespec trc, char* msg);
#endif  /* LOG_H */