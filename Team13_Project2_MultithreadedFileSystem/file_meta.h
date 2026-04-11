#ifndef FILE_META_H
#define FILE_META_H

typedef struct {
    const char *source;
    const char *dest;
} copy_args_t;

void display_metadata(const char *filename);
void copy_file_task(void *arg);

#endif