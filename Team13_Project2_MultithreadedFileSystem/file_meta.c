#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include "rwlock_manager.h"
#include "logger.h"
#include "file_meta.h"

void display_metadata(const char *filename) {
    fs_read_lock();
    
    struct stat st;
    if (stat(filename, &st) == 0) {
        printf("File: %s\n", filename);
        printf("Size: %ld bytes\n", st.st_size);
        printf("Permissions: %o\n", st.st_mode & 0777);
        printf("Last Modified: %s", ctime(&st.st_mtime));
        log_operation("METADATA: %s", filename);
    } else {
        perror("stat");
        log_operation("METADATA ERROR: %s", filename);
    }
    
    fs_unlock();
}

void copy_file_task(void *arg) {
    copy_args_t *args = (copy_args_t *)arg;
    const char *source = args->source;
    const char *dest = args->dest;
    
    // Read lock for source
    fs_read_lock();
    FILE *src = fopen(source, "rb");
    fs_unlock();
    
    if (!src) {
        perror("Failed to open source file");
        log_operation("COPY ERROR: source %s", source);
        free(args);
        return;
    }
    
    // Write lock for destination
    fs_write_lock();
    FILE *dst = fopen(dest, "wb");
    
    if (!dst) {
        fclose(src);
        fs_unlock();
        perror("Failed to open destination file");
        log_operation("COPY ERROR: dest %s", dest);
        free(args);
        return;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    fs_unlock();
    
    log_operation("COPY: %s to %s", source, dest);
    free(args);
}