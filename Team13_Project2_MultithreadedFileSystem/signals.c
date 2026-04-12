#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void handle_sigint(int sig) {
    (void)sig;
    printf("\n[Signal] Graceful shutdown initiated...\n");
    exit(0);
}

void handle_sigusr1(int sig) {
    (void)sig;
    printf("\n[Status] System is idle/processing...\n");
}

void setup_signals(void) {
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_sigusr1);
}
