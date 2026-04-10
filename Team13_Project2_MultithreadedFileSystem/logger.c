#include "logger.h"
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int logger_init(void) {
    pthread_mutex_lock(&log_mutex);
    if (!log_file) {
        log_file = fopen("ops.log", "a");
    }
    pthread_mutex_unlock(&log_mutex);
    return (log_file != NULL) ? 0 : -1;
}

void log_operation(const char *format, ...) {
    if (!log_file) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    pthread_mutex_lock(&log_mutex);
    
    // Print timestamp
    fprintf(log_file, "[%s.%03d] ", time_buf, (int)(tv.tv_usec / 1000));
    
    // Print user message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file); // Ensure it's written immediately
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}
