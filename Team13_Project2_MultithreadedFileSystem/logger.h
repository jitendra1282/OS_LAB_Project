#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

/**
 * Initializes the logger. Opens the ops.log file.
 * Returns 0 on success, -1 on error.
 */
int logger_init(void);

/**
 * Writes a formatted message to ops.log with a timestamp.
 * Thread-safe.
 */
void log_operation(const char *format, ...);

/**
 * Closes the logger.
 */
void logger_close(void);

#endif // LOGGER_H
