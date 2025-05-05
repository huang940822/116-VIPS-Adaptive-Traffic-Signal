#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error_status.h"
#include "log.h"
#include "timer_event.h"
#include "typedefine.h"

LOG_USE_MODULE(MIDDLEWARE);

#define CORE_MODULE_NAME    "MIDDLEWARE"

pthread_mutex_t mutex_log_file_ptr = PTHREAD_MUTEX_INITIALIZER;

char log_file_name[LOG_FILE_NAME_LEN];
FILE *log_file_ptr;

timer_t log_file_name_update_timer_id;
uint8_t log_file_name_update_num = TIMER_EVENT_LOG_FILE_NAME_UPDATE;

static log_level_t current_log_level = LOG_LEVEL_INFO;
const char *log_level_strs[] = {
    [LOG_LEVEL_TRACE] = "TRACE",
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARN] = "WARN",
    [LOG_LEVEL_ERROR] = "ERROR",
    [LOG_LEVEL_FATAL] = "FATAL",
};

/**
 * @brief 初始化日誌檔案
 *
 * 此函式用於初始化日誌檔案，包括設定檔案名稱、建立檔案、以及檢查檔案是否成功開啟。
 * 檔案名稱會根據當地時間生成，並附加在指定的目錄下。
 * 若檔案開啟失敗，會記錄錯誤訊息。
 * 同時，會建立一個計時器事件，用於定期更新日誌檔案名稱。
 */
void log_file_init()
{
    // timestamp
    time_t rawtime;
    struct tm localTime;
    time(&rawtime);
    localtime_r(&rawtime, &localTime);
    strftime(log_file_name, LOG_FILE_NAME_LEN, LOG_FILE_NAME_FORMAT, &localTime);

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncpy(file_path, LOG_DIR, sizeof(LOG_DIR));
    strncat(file_path, log_file_name, sizeof(file_path));
    strcat(file_path, ".log");

    log_file_ptr = fopen(file_path, "a+");

    if (log_file_ptr == NULL) {
        LOG_MSG_FATAL("error opening %s", file_path);
    } else {
        LOG_MSG_INFO("%s opened successfully", file_path);
    }

    /* log file name update timer event */
    create_timer(&log_file_name_update_timer_id, &log_file_name_update_num,
            timer_event_handler);
    set_timer(log_file_name_update_timer_id, 15, 0, 1, 0);
}

/**
 * @brief 更新日誌檔案名稱
 *
 * 此函式用於更新日誌檔案的名稱。它根據當前的時間戳記生成新的檔案名稱，
 * 並將舊的日誌檔案關閉，打開新的日誌檔案。如果打開或關閉檔案時發生錯誤，
 * 將記錄錯誤訊息到日誌檔案中。
 */
void log_file_name_update()
{
    // timestamp
    time_t rawtime;
    struct tm localTime;
    char buffer[LOG_FILE_NAME_LEN];
    time(&rawtime);
    localtime_r(&rawtime, &localTime);
    strftime(buffer, LOG_FILE_NAME_LEN, LOG_FILE_NAME_FORMAT, &localTime);

    if (strncmp(buffer, log_file_name, LOG_FILE_NAME_LEN) != 0) {
        /* Get new file path */
        char file_path[255];
        memset(file_path, 0, sizeof(file_path));
        strncpy(file_path, LOG_DIR, sizeof(LOG_DIR));
        strncat(file_path, buffer, sizeof(file_path));
        strcat(file_path, ".log");

        FILE *new_log_file_ptr;
        FILE *tmp_log_file_ptr;

        new_log_file_ptr = fopen(file_path, "a+");

        if (new_log_file_ptr == NULL) {
            LOG_MSG_FATAL("error opening %s", file_path);
        } else {
            LOG_MSG_INFO("%s opened successfully", file_path);

            pthread_mutex_lock(&mutex_log_file_ptr);
            tmp_log_file_ptr = log_file_ptr;
            log_file_ptr = new_log_file_ptr;
            pthread_mutex_unlock(&mutex_log_file_ptr);

            /* Get old file path */
            memset(file_path, 0, sizeof(file_path));
            strncpy(file_path, LOG_DIR, sizeof(LOG_DIR));
            strncat(file_path, log_file_name, sizeof(file_path));
            strcat(file_path, ".log");

            if (fclose(tmp_log_file_ptr) == 0) {
                LOG_MSG_INFO("%s closed successfully", file_path);
            } else {
                LOG_MSG_FATAL("error closing %s", file_path);
            }

            strncpy(log_file_name, buffer, LOG_FILE_NAME_LEN);
        }
    }
    return;
}

/**
 * @brief 設定日誌等級。
 *
 * 此函數用於設定當前的日誌等級。
 *
 * @param level 要設定的日誌等級
 */
void log_set_level(log_level_t level)
{
    current_log_level = level;
}

/**
 * @brief 寫入日誌訊息至檔案和標準輸出
 *
 * 此函式將日誌訊息寫入檔案和標準輸出。
 * 若寫入檔案或刷新緩衝區時發生錯誤，將設置磁碟錯誤標誌並終止程式執行。
 *
 * @param[in] time_str_ptr 時間字串指標
 * @param[in] level 日誌等級
 * @param[in] file 檔案名稱
 * @param[in] line 行號
 * @param[in] log_module_name 日誌模組名稱
 * @param[in] log_content_ptr 日誌內容指標
 */
static void _log_appliction(char *time_str_ptr, log_level_t level, const char *file, int line, const char *log_module_name, char *log_content_ptr) {
    pthread_mutex_lock(&mutex_log_file_ptr);
    if (fprintf(log_file_ptr, "[%s][%s][%s][%s:%d] - %s\n",
            time_str_ptr, log_level_strs[current_log_level], log_module_name, file, line, log_content_ptr) < 0) {
        set_disk_error();
        perror("_log_appliction: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fflush(log_file_ptr) != 0) {
        perror("_log_appliction: fflush");
        exit(errno);
    }
    pthread_mutex_unlock(&mutex_log_file_ptr);
}

/**
 * @brief 用於記錄日誌的核心函數。
 *
 * 此函數將日誌訊息寫入檔案中。
 * 如果寫入檔案或刷新緩衝區時發生錯誤，將設置磁碟錯誤標誌並終止程式執行。
 *
 * @param[in] time_str_ptr 指向時間字串的指標
 * @param[in] level 日誌級別
 * @param[in] file 檔案名
 * @param[in] line 行號
 * @param[in] log_content_ptr 指向日誌內容的指標
 */
static void _log_core(char *time_str_ptr, log_level_t level, const char *log_module_name, char *log_content_ptr) {
    pthread_mutex_lock(&mutex_log_file_ptr);
    if (fprintf(log_file_ptr, "[%s][%s][%s] - %s\n",
            time_str_ptr, log_level_strs[current_log_level], log_module_name, log_content_ptr) < 0) {
        set_disk_error();
        perror("_log_core: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fflush(log_file_ptr) != 0) {
        perror("_log_core: fflush");
        exit(errno);
    }
    pthread_mutex_unlock(&mutex_log_file_ptr);
}

/**
 * @brief 寫入日誌檔案
 *
 * 此函式用於將日誌訊息寫入日誌檔案中。它會根據當前的日誌等級和模組名稱，
 * 將訊息格式化並寫入檔案中。
 *
 * @param[in] level 日誌等級
 * @param[in] file 檔案名稱
 * @param[in] line 行號
 * @param[in] log_module_name 模組名稱
 * @param[in] format 格式化字串
 * @param[in] ... 其他參數
 *
 * @note 此函式會自動加鎖以確保多執行緒環境下的安全性。
 *      如果寫入檔案失敗，會記錄錯誤訊息並終止程式。
 *      此外，會將時間戳記和內容寫入檔案中。
 * @note 格式為 [%(asctime)s][%(levelname)s][%(modulename)s][%(filename)s:%(lineno)d] - %(message)s
 */
void log_file_write(log_level_t level, const char *file, int line, const char *log_module_name, const char *format, ...)
{
    /* Early Return */
    if (level < current_log_level) {
        return;
    }

    /* 產生 timestamp */
    time_t rawtime;
    struct tm localTime;
    char time_format_str[20];
    memset(time_format_str, 0, sizeof(time_format_str));
    time(&rawtime);
    localtime_r(&rawtime, &localTime); // 轉換成本地時間表示的分解時間
    strftime(time_format_str, 20, "%Y-%m-%d %H:%M:%S", &localTime);

    /* 產生 content 字串 */
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    va_list list;
    va_start(list, format);
    vsnprintf(log_content, LOG_CONTENT_LEN, format, list);
    va_end(list);

    if (strncmp(log_module_name, CORE_MODULE_NAME, sizeof(CORE_MODULE_NAME)) != 0) {
        _log_appliction(time_format_str, level, file, line, log_module_name, log_content);
    }
    else {
        _log_core(time_format_str, level, log_module_name, log_content);
    }
}

/**
 * @brief 在日誌內容後附加格式化的字串。
 *
 * 此函式將格式化的字串附加到指定的日誌內容後面。
 *
 * @param[in,out] log_content 日誌內容
 * @param[in] max_len 日誌內容的最大長度
 * @param[in] fmt 格式化字串
 * @param[in] ... 格式化字串的參數
 * @return 成功附加字串後的日誌內容長度，若失敗則返回負數
 */
int log_appendf(char *log_content, size_t max_len, const char *fmt, ...)
{
    char tmp_log_content[LOG_CONTENT_LEN + 1];
    memset(tmp_log_content, 0, sizeof(tmp_log_content));

    va_list list;
    va_start(list, fmt);
    int ret = vsnprintf(tmp_log_content, LOG_CONTENT_LEN, fmt, list);
    va_end(list);
    /* Failed to format string */
    if (ret < 0) {
        return (-1);
    }

    size_t _len = strlen(log_content);
    ret = snprintf(log_content + _len, max_len - _len, "%s", tmp_log_content);
    /* Failed to concat string to log content string */
    if (ret < 0) {
        return (-1);
    }

    /* Check if the log content length exceeds the maximum length */
    if (ret >= max_len) {
        return (-1);
    }

    return ret;
}

/**
 * @brief 寫入嚴重錯誤日誌的函數。
 *
 * 這個函數將嚴重錯誤的訊息寫入日誌檔案中，並在必要時處理磁碟錯誤。
 *
 * @param[in] format 格式化字串，用於指定錯誤訊息的格式。
 * @param[in] ... 可變參數，用於填充格式化字串中的佔位符。
 */
void log_file_write_fatal_error(const char *format, ...)
{
    // timestamp
    time_t rawtime;
    struct tm localTime;
    char buffer[20];
    memset(buffer, 0, sizeof(buffer));
    time(&rawtime);
    localtime_r(&rawtime, &localTime);
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &localTime);

    // content
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    va_list list;
    va_start(list, format);
    vsnprintf(log_content, LOG_CONTENT_LEN, format, list);
    va_end(list);

    pthread_mutex_lock(&mutex_log_file_ptr);
    if (fprintf(log_file_ptr, "%s\n", buffer) < 0) {
        set_disk_error();
        perror("log_file_write_fatal_error: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fprintf(log_file_ptr, "fatal error: \n%s\n", log_content) < 0) {
        set_disk_error();
        perror("log_file_write_fatal_error: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fflush(log_file_ptr) != 0) {
        perror("log_file_write_fatal_error: fflush");
        exit(errno);
    }
    pthread_mutex_unlock(&mutex_log_file_ptr);
}


/**
 * @brief 使用錯誤碼寫入日誌檔案。
 *
 * @param[in] format 格式化字串，用於生成日誌內容。
 * @param[in] ... 可變參數列表，用於格式化日誌內容。
 * @return int 返回值為 0。
 */
int log_file_write_with_errno(const char *format, ...)
{
    if( ENABLE_FATAL_WITH_ERR_CODE_LOG ){
        if( SWITCH_FATAL_WITH_ERR_CODE_LOG_TO_PRINT ){
            char* errno_str = strerror(errno);
            if( !errno_str ) errno_str = "undefined/zero errno";

            // timestamp
            time_t rawtime;
            struct tm *info;
            char buffer[20];
            memset(buffer, 0, sizeof(buffer));
            time(&rawtime);
            info = localtime(&rawtime);
            strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);

            // content
            char log_content[LOG_CONTENT_LEN + 1];
            memset(log_content, 0, sizeof(log_content));
            va_list list;
            va_start(list, format);
            vsnprintf(log_content, LOG_CONTENT_LEN, format, list);
            va_end(list);

            fprintf(stderr, "%s\n", buffer);
            fprintf(stderr, "strerror() shows: %s\n", errno_str);
            fprintf(stderr, "fatal error: \n%s\n", log_content);
            fflush(stderr);
        }
        else{
            ;//log to a file
        }
    }

    return 0;
}


/**
 * @brief 計算兩個 timespec 結構體之間的差值。
 *
 * @param[in] bgn 開始時間 timespec 結構體。
 * @param[in] end 結束時間 timespec 結構體。
 * @return 兩個 timespec 結構體之間的差值。
 */
struct timespec get_timespec_diff(struct timespec bgn, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - bgn.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec - bgn.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec - bgn.tv_nsec;
    }
    else {
        temp.tv_sec = end.tv_sec - bgn.tv_sec;
        temp.tv_nsec = end.tv_nsec - bgn.tv_nsec;
    }
    return temp;
}

/**
 * @brief 計算兩個時間結構體之間的微秒差異。
 *
 * @param[in] bgn 開始時間結構體
 * @param[in] end 結束時間結構體
 * @return 兩個時間結構體之間的微秒差異
 */
uint32_t get_us_diff(struct timespec bgn, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - bgn.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec - bgn.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec - bgn.tv_nsec;
    }
    else {
        temp.tv_sec = end.tv_sec - bgn.tv_sec;
        temp.tv_nsec = end.tv_nsec - bgn.tv_nsec;
    }
    return (uint32_t)((temp.tv_sec)*1000000 + (temp.tv_nsec)/1000);
}

/**
 * @brief 紀錄當前的 timespec
 *
 * 此函數用於獲取當前的 timespec，並將其存儲在指定的結構體中。
 *
 * @param[out] now_p 指向 timespec 結構體的指針，用於存儲當前的 timespec
 */
void record_current_timespec(struct timespec* now_p)
{
    clock_gettime(CLOCK_MONOTONIC, now_p);
}

/**
 * @brief 將 timespec 結構的時間差輸出到 stderr。
 *
 * 此函數將兩個 timespec 結構表示的時間差輸出到 stderr，並可選擇性地附加訊息。
 *
 * @param[in] bgn 開始時間的 timespec 結構。
 * @param[in] end 結束時間的 timespec 結構。
 * @param[in] msg 附加的訊息，可選。
 */
void print_timespec_to_stderr(struct timespec bgn, struct timespec end, char* msg)
{
    if(msg)
        fprintf(stderr, "%s ", msg);

    struct timespec temp;
    if ((end.tv_nsec - bgn.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec - bgn.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec - bgn.tv_nsec;
    }
    else {
        temp.tv_sec = end.tv_sec - bgn.tv_sec;
        temp.tv_nsec = end.tv_nsec - bgn.tv_nsec;
    }

    fprintf(stderr, "s: %ld , ns: %ld\n", temp.tv_sec, temp.tv_nsec);
}

/**
 * @brief 將單一的 timespec 結構輸出到標準輸出
 *
 * 此函數將 timespec 結構的秒數和納秒數輸出到標準輸出。
 *
 * @param[in] trc 要輸出的 timespec 結構
 * @param[out] msg 附加的訊息，可選參數
 */
void print_single_timespec_to_stdout(struct timespec trc, char* msg)
{
    if(msg)
        fprintf(stdout, "%s ", msg);

    fprintf(stdout, "s: %ld , ns: %ld\n", trc.tv_sec, trc.tv_nsec);
}