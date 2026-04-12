/*
 * signals.h — Signal Handling Module
 *
 * Manages graceful shutdown via SIGINT and status reporting via SIGUSR1.
 *
 * Features:
 *   - SIGINT (Ctrl+C): Graceful shutdown — logs message and cleans up
 *   - SIGUSR1: Print current operation status from ops.log tail
 *   - Thread-safe signal registration and handling
 */

#ifndef SIGNALS_H
#define SIGNALS_H

/**
 * Initializes signal handlers for SIGINT and SIGUSR1
 * Returns 0 on success, -1 on error
 */
int signals_init(void);

/**
 * Signal handler for SIGINT (Ctrl+C) — graceful shutdown
 */
void signal_handle_sigint(int sig);

/**
 * Signal handler for SIGUSR1 — print operation status
 */
void signal_handle_sigusr1(int sig);

/**
 * Flag to indicate if shutdown was requested
 */
extern volatile int g_shutdown_requested;

#endif /* SIGNALS_H */
