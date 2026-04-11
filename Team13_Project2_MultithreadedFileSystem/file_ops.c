#include <unistd.h>
#include <errno.h>
#include "rwlock_manager.h"
#include "logger.h"
#include "file_ops.h"

int delete_file(const char *filename) {
    fs_write_lock();
    if (unlink(filename) == 0) {
        log_operation("SUCCESS: Deleted file %s", filename);
        fs_unlock();
        return 0;
    } else {
        int err = errno;
        log_operation("ERROR: Failed to delete file %s", filename);
        fs_unlock();
        return err;
    }
}

int rename_file(const char *old_name, const char *new_name) {
    fs_write_lock();
    if (rename(old_name, new_name) == 0) {
        log_operation("SUCCESS: Renamed file %s to %s", old_name, new_name);
        fs_unlock();
        return 0;
    } else {
        int err = errno;
        log_operation("ERROR: Failed to rename %s to %s", old_name, new_name);
        fs_unlock();
        return err;
    }
}