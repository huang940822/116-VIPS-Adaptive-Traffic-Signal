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


#define LOG_MODULE_NAME     __log_module_name
#define LOG_MARK            __FILE__,__LINE__,LOG_MODULE_NAME

#define LOG_USE_MODULE(foo) \
    static char * const __log_module_name = #foo;
#define LOG_MSG_TRACE(fmt, ...)                 log_file_write(LOG_LEVEL_TRACE, LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_DEBUG(fmt, ...)                 log_file_write(LOG_LEVEL_DEBUG, LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_INFO(fmt, ...)                  log_file_write(LOG_LEVEL_INFO,  LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_WARN(fmt, ...)                  log_file_write(LOG_LEVEL_WARN,  LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_ERROR(fmt, ...)                 log_file_write(LOG_LEVEL_ERROR, LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_FATAL(fmt, ...)                 log_file_write(LOG_LEVEL_FATAL, LOG_MARK, fmt, ##__VA_ARGS__)
#define LOG_MSG_APPEND(log_content, fmt, ...)   log_appendf(log_content, LOG_CONTENT_LEN, fmt, ##__VA_ARGS__)

typedef enum {
    LOG_LEVEL_TRACE = 0,    /**< 封包相關資訊 */
    LOG_LEVEL_DEBUG,        /**< 詳細運行資訊 */
    LOG_LEVEL_INFO,         /**< 一般運行資訊 */
    LOG_LEVEL_WARN,         /**< 異常資訊但不影響運作 */
    LOG_LEVEL_ERROR,        /**< 異常資訊且影響運作但不會導致程式中止 */
    LOG_LEVEL_FATAL,        /**< 異常導致程式中止 */
} log_level_t;

extern const char *log_level_strs[];

void log_file_init();
void log_file_name_update();
void log_set_level(log_level_t level);
void log_file_write(log_level_t level, const char *file, int line, const char *log_module_name, const char *format, ...);
int log_appendf(char *log_content, size_t max_len, const char *fmt, ...);

/* FIXME: Remove unused function in ticket #36 */
// void log_file_write_fatal_error(char *content);
void log_file_write_fatal_error(const char *format, ...);

/* FIXME: Remove unused function in ticket #36 */
#define ENABLE_FATAL_WITH_ERR_CODE_LOG 1
#define SWITCH_FATAL_WITH_ERR_CODE_LOG_TO_PRINT 1
int log_file_write_with_errno(const char *format, ...);

/* FIXME: Remove unused function in ticket #36 */
__attribute__((unused)) struct timespec get_timespec_diff(struct timespec bgn, struct timespec end);
__attribute__((unused)) uint32_t get_us_diff(struct timespec bgn, struct timespec end);
__attribute__((unused)) void record_current_timespec(struct timespec* now_p);
__attribute__((unused)) void print_timespec_to_stderr(struct timespec bgn, struct timespec end, char* msg);
__attribute__((unused)) void print_single_timespec_to_stdout(struct timespec trc, char* msg);

#endif  /* LOG_H */