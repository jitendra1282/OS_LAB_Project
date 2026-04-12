/*
 * main.c — Multithreaded File Management System
 *
 * Member 6 — CLI + Error Handling
 * Interactive colored terminal menu with proper error handling
 * Ties together all modules: file_rw, file_ops, file_meta, compress, signals
 *
 * Usage:
 *   ./fs_sim                    (Interactive mode)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cli.h"
#include "logger.h"
#include "signals.h"

int main(void)
{
    /* ── Initialize subsystems ─────────────────────────────– */
    
    /* Setup signal handlers first (SIGINT for graceful shutdown, SIGUSR1 for status) */
    setup_signals();

    /* Initialize logger */
    if (logger_init() != 0) {
        fprintf(stderr, ANSI_COLOR_RED "ERROR: Failed to initialize logger\n" ANSI_COLOR_RESET);
        return 1;
    }
    log_operation("====== SYSTEM START ======");

    /* Print banner */
    printf(ANSI_COLOR_CYAN "\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  Multithreaded File Management System     ║\n");
    printf("║  Team 13 — Operating Systems Lab Project  ║\n");
    printf("║  All 6 Members: Thread Pool, Logging,     ║\n");
    printf("║  File I/O, Metadata, Compression, CLI     ║\n");
    printf("╚═══════════════════════════════════════════╝\n");
    printf(ANSI_COLOR_RESET "\n");

    printf("✓ System initialized\n");
    printf("✓ Signal handlers ready (Ctrl+C to exit, kill -USR1 for status)\n");
    printf("✓ Logger active (logging to ops.log)\n\n");

    /* ── Run interactive CLI ────────────────────────────────– */
    cli_run();

    /* ── Shutdown ───────────────────────────────────────────– */
    log_operation("====== SYSTEM STOP ======");
    logger_close();

    printf(ANSI_COLOR_GREEN "✓ All resources cleaned up\n" ANSI_COLOR_RESET);
    return 0;
}
