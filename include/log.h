#ifndef LOG_H
#define LOG_H

#include "typedefine.h"

#define LOG_DIR FILE_PATH "log/"

// #define LOG_FILE_NAME_FORMAT "%Y-%m-%d %H:%M"
// #define LOG_FILE_NAME_LEN 17

#define LOG_FILE_NAME_FORMAT "%Y-%m-%d %H"
#define LOG_FILE_NAME_LEN 14

#define ERR_MSG_SZ 128
#define LOG_CONTENT_LEN 2048

void log_file_init();
void log_file_name_update();
void log_file_write(const char *format, ...);
// void log_file_write_fatal_error(char *content);
void log_file_write_fatal_error(const char *format, ...);

#define ENABLE_LOG 1
#define ENABLE_FATAL_LOG 1
#define SWITCH_LOG_TO_PRINT 1
#define SWITCH_FATAL_LOG_TO_PRINT 1
int log_file_write_fatal_with_errno(const char *format, ...);

#endif  /* LOG_H */