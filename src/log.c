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

pthread_mutex_t mutex_log_file_ptr = PTHREAD_MUTEX_INITIALIZER;

char log_file_name[LOG_FILE_NAME_LEN];
FILE *log_file_ptr;

timer_t log_file_name_update_timer_id;
uint8_t log_file_name_update_num = TIMER_EVENT_LOG_FILE_NAME_UPDATE;

void log_file_init()
{
    // timestamp
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(log_file_name, LOG_FILE_NAME_LEN, LOG_FILE_NAME_FORMAT, info);

    /* Get file path */
    char file_path[255];
    memset(file_path, 0, sizeof(file_path));
    strncpy(file_path, LOG_DIR, sizeof(LOG_DIR));
    strncat(file_path, log_file_name, sizeof(file_path));
    strcat(file_path, ".log");

    log_file_ptr = fopen(file_path, "a+");

    if (log_file_ptr == NULL) {
        log_file_write_fatal_error("error opening %s", file_path);
    } else {
        log_file_write("%s opened successfully", file_path);
    }

    /* log file name update timer event */
    create_timer(&log_file_name_update_timer_id, &log_file_name_update_num,
                 timer_event_handler);
    set_timer(log_file_name_update_timer_id, 15, 0, 1, 0);
}

void log_file_name_update()
{
    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[LOG_FILE_NAME_LEN];
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(buffer, LOG_FILE_NAME_LEN, LOG_FILE_NAME_FORMAT, info);

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
            log_file_write_fatal_error("error opening %s", file_path);
        } else {
            log_file_write("%s opened successfully", file_path);

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
                log_file_write("%s closed successfully", file_path);
            } else {
                log_file_write_fatal_error("error closing %s", file_path);
            }

            strncpy(log_file_name, buffer, LOG_FILE_NAME_LEN);
        }
    }
    return;
}

void log_file_write(const char *format, ...)
{
    // timestamp
    time_t rawtime;
    struct tm *info;
    char buffer[20];
    memset(buffer, 0, sizeof(buffer));
    time(&rawtime);
    info = localtime(&rawtime);  // 轉換成本地時間表示的分解時間
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", info);

    // content
    char log_content[LOG_CONTENT_LEN + 1];
    memset(log_content, 0, sizeof(log_content));
    va_list list;
    va_start(list, format);
    vsnprintf(log_content, LOG_CONTENT_LEN, format, list);
    va_end(list);

    pthread_mutex_lock(&mutex_log_file_ptr);
    if (fprintf(log_file_ptr, "\e[1;4;32m%s\n\e[m", buffer) < 0) {
        set_disk_error();
        perror("log_file_write: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fprintf(log_file_ptr, "%s\n", log_content) < 0) {
        set_disk_error();
        perror("log_file_write: fprintf");
        exit(errno);
    } else {
        clear_disk_error();
    }
    if (fflush(log_file_ptr) != 0) {
        perror("log_file_write: fflush");
        exit(errno);
    }
    pthread_mutex_unlock(&mutex_log_file_ptr);
}

void log_file_write_fatal_error(const char *format, ...)
{
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


int log_file_write_fatal_with_errno(const char *format, ...)
{   
    if( ENABLE_FATAL_LOG ){
        if( SWITCH_FATAL_LOG_TO_PRINT ){
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
            fprintf(stderr, "strerror is %s\n", errno_str);
            fprintf(stderr, "fatal error: \n%s\n", log_content);
            fflush(stderr);
        }
        else{
            ;//TODO: log to a file
        }
    }

    return 0;
}
