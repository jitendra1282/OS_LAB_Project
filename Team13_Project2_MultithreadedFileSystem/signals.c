/*
 * signals.c — Signal Handling Implementation
 *
 * Handles SIGINT for graceful shutdown and SIGUSR1 for status reporting.
 */

#include "signals.h"
#include "logger.h"

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

/* Global shutdown flag */
volatile int g_shutdown_requested = 0;

/* ── SIGINT handler — graceful shutdown ────────────────────── */
void signal_handle_sigint(int sig)
{
    (void)sig;  /* suppress unused parameter warning */

    g_shutdown_requested = 1;

    /* Log the shutdown event */
    log_operation("════ SIGINT received — GRACEFUL SHUTDOWN ════");
    log_operation("All workers will finish current tasks and exit");

    fprintf(stderr, "\n\n───────────────────────────────────────\n");
    fprintf(stderr, "INTERRUPT: SIGINT received — shutting down gracefully\n");
    fprintf(stderr, "───────────────────────────────────────\n\n");
}

/* ── SIGUSR1 handler — print operation status ──────────────── */
void signal_handle_sigusr1(int sig)
{
    (void)sig;  /* suppress unused parameter warning */

    fprintf(stderr, "\n\n╔══════════════════════════════════════════╗\n");
    fprintf(stderr, "║  USER SIGNAL: SIGUSR1 — Current Status    ║\n");
    fprintf(stderr, "╚══════════════════════════════════════════╝\n\n");

    log_operation("════ SIGUSR1 received — STATUS REPORT ════");

    /* Try to display tail of ops.log */
    FILE *fp = fopen("ops.log", "r");
    if (fp) {
        /* Count lines and find last 10 */
        char line[512];
        char lines[10][512];
        int count = 0;
        int idx = 0;

        memset(lines, 0, sizeof(lines));

        while (fgets(line, sizeof(line), fp) && count < 10) {
            strncpy(lines[idx], line, sizeof(lines[idx]) - 1);
            lines[idx][sizeof(lines[idx]) - 1] = '\0';
            idx = (idx + 1) % 10;
            count++;
        }

        fprintf(stderr, "─── Last %d operations in ops.log ─────────\n\n", count);

        /* Print in chronological order */
        if (count < 10) {
            for (int i = 0; i < count; i++) {
                fprintf(stderr, "%s", lines[i]);
            }
        } else {
            for (int i = 0; i < 10; i++) {
                if (lines[i][0] != '\0') {
                    fprintf(stderr, "%s", lines[i]);
                }
            }
        }

        fclose(fp);
    } else {
        fprintf(stderr, "    (ops.log not yet created)\n");
    }

    fprintf(stderr, "\n──────────────────────────────────────────\n\n");
}

/* ── Initialize signal handlers ───────────────────────────── */
int signals_init(void)
{
    struct sigaction sa_int, sa_usr1;

    /* Set up SIGINT handler (graceful shutdown) */
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = signal_handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        fprintf(stderr, "ERROR: Failed to set SIGINT handler\n");
        return -1;
    }

    /* Set up SIGUSR1 handler (status reporting) */
    memset(&sa_usr1, 0, sizeof(sa_usr1));
    sa_usr1.sa_handler = signal_handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        fprintf(stderr, "ERROR: Failed to set SIGUSR1 handler\n");
        return -1;
    }

    log_operation("SYSTEM: Signal handlers initialized (SIGINT, SIGUSR1)");

    fprintf(stderr, "═══════════════════════════════════════════════\n");
    fprintf(stderr, "  Signal Handlers Registered:\n");
    fprintf(stderr, "  • SIGINT (Ctrl+C)  → Graceful shutdown\n");
    fprintf(stderr, "  • SIGUSR1 (kill -USR1)  → Print status\n");
    fprintf(stderr, "═══════════════════════════════════════════════\n\n");

    return 0;
}
