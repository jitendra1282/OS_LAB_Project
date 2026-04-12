#include "cli.h"
#include "file_ops.h"
#include "file_meta.h"
#include "compress.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ── Expand tilde in path ──────────────────────────────────── */
static char *expand_tilde(const char *path)
{
    if (!path || path[0] != '~') {
        return strdup(path);
    }

    /* Get home directory */
    const char *home = getenv("HOME");
    if (!home) {
        return strdup(path);  /* fallback if HOME not set */
    }

    /* Allocate space for expanded path */
    size_t home_len = strlen(home);
    size_t path_len = strlen(path);
    size_t total_len = home_len + path_len;  /* ~ is replaced by home, so we need space for home + rest of path */

    char *expanded = malloc(total_len);
    if (!expanded) {
        return strdup(path);
    }

    /* Copy home directory */
    strcpy(expanded, home);

    /* Append the rest of the path (skip the ~) */
    if (path_len > 1) {  /* if there's more than just ~ */
        strcat(expanded, path + 1);
    }

    return expanded;
}

/* ── Input validation ───────────────────────────────────────── */
int cli_validate_file(const char *filepath)
{
    if (!filepath || strlen(filepath) == 0) {
        printf(ANSI_COLOR_RED "ERROR: Empty file path\n" ANSI_COLOR_RESET);
        return CLI_INVALID_INPUT;
    }

    if (access(filepath, F_OK) != 0) {
        printf(ANSI_COLOR_RED "ERROR: File '%s' not found\n" ANSI_COLOR_RESET, filepath);
        return CLI_NOT_FOUND;
    }

    if (access(filepath, R_OK) != 0) {
        printf(ANSI_COLOR_RED "ERROR: No read permission for '%s'\n" ANSI_COLOR_RESET, filepath);
        return CLI_PERMISSION_ERROR;
    }

    return CLI_SUCCESS;
}

/* ── Get filepath from user ────────────────────────────────── */
char *cli_get_filepath(const char *prompt)
{
    char *filepath = malloc(256);
    if (!filepath) {
        printf(ANSI_COLOR_RED "ERROR: Memory allocation failed\n" ANSI_COLOR_RESET);
        return NULL;
    }

    printf("%s", prompt);
    if (fgets(filepath, 256, stdin) == NULL) {
        printf(ANSI_COLOR_RED "ERROR: Failed to read input\n" ANSI_COLOR_RESET);
        free(filepath);
        return NULL;
    }

    /* Remove trailing newline */
    size_t len = strlen(filepath);
    if (len > 0 && filepath[len - 1] == '\n') {
        filepath[len - 1] = '\0';
    }

    /* Expand tilde if present */
    char *expanded = expand_tilde(filepath);
    free(filepath);

    return expanded;
}

/* ── Get user choice ────────────────────────────────────────── */
int cli_get_choice(void)
{
    int choice;
    if (scanf("%d", &choice) != 1) {
        printf(ANSI_COLOR_RED "ERROR: Invalid input. Please enter a number.\n" ANSI_COLOR_RESET);
        while (getchar() != '\n');  /* Clear input buffer */
        return -1;
    }
    while (getchar() != '\n');  /* Clear remaining newline */

    if (choice < 1 || choice > 5) {
        printf(ANSI_COLOR_RED "ERROR: Choice must be 1-5\n" ANSI_COLOR_RESET);
        return -1;
    }

    return choice;
}

/* ── Display menu ───────────────────────────────────────────– */
void cli_show_menu(void)
{
    printf(ANSI_COLOR_CYAN "╔════════════════════════════════════════╗\n");
    printf("║  Multi-Threaded File Manager (Team 13) ║\n");
    printf("╚════════════════════════════════════════╝" ANSI_COLOR_RESET "\n\n");

    printf(ANSI_COLOR_YELLOW "Available Operations:\n" ANSI_COLOR_RESET);
    printf("  " ANSI_COLOR_GREEN "1." ANSI_COLOR_RESET " Delete File\n");
    printf("  " ANSI_COLOR_GREEN "2." ANSI_COLOR_RESET " View Metadata\n");
    printf("  " ANSI_COLOR_GREEN "3." ANSI_COLOR_RESET " Rename File\n");
    printf("  " ANSI_COLOR_GREEN "4." ANSI_COLOR_RESET " Compress File\n");
    printf("  " ANSI_COLOR_GREEN "5." ANSI_COLOR_RESET " Exit\n\n");

    printf(ANSI_COLOR_CYAN "Choice: " ANSI_COLOR_RESET);
}

/* ── Delete file operation ───────────────────────────────────– */
static void cli_delete_file(void)
{
    printf(ANSI_COLOR_MAGENTA "\n--- Delete File ---\n" ANSI_COLOR_RESET);

    char *filepath = cli_get_filepath("Enter file path: ");
    if (!filepath) {
        return;
    }

    int val = cli_validate_file(filepath);
    if (val != CLI_SUCCESS) {
        free(filepath);
        return;
    }

    if (delete_file(filepath) == 0) {
        printf(ANSI_COLOR_GREEN "✓ File '%s' deleted successfully\n" ANSI_COLOR_RESET, filepath);
        log_operation("CLI: Deleted file '%s'", filepath);
    } else {
        printf(ANSI_COLOR_RED "✗ Failed to delete '%s'\n" ANSI_COLOR_RESET, filepath);
        log_operation("CLI: Failed to delete '%s'", filepath);
    }

    free(filepath);
}

/* ── View metadata operation ────────────────────────────────– */
static void cli_view_metadata(void)
{
    printf(ANSI_COLOR_MAGENTA "\n--- View Metadata ---\n" ANSI_COLOR_RESET);

    char *filepath = cli_get_filepath("Enter file path: ");
    if (!filepath) {
        return;
    }

    int val = cli_validate_file(filepath);
    if (val != CLI_SUCCESS) {
        free(filepath);
        return;
    }

    printf(ANSI_COLOR_GREEN "Fetching metadata for '%s'...\n" ANSI_COLOR_RESET, filepath);
    display_metadata(filepath);
    log_operation("CLI: Displayed metadata for '%s'", filepath);

    free(filepath);
}

/* ── Rename file operation ───────────────────────────────────– */
static void cli_rename_file(void)
{
    printf(ANSI_COLOR_MAGENTA "\n--- Rename File ---\n" ANSI_COLOR_RESET);

    char *old_path = cli_get_filepath("Enter old file path: ");
    if (!old_path) {
        return;
    }

    int val = cli_validate_file(old_path);
    if (val != CLI_SUCCESS) {
        free(old_path);
        return;
    }

    char *new_path = cli_get_filepath("Enter new file path: ");
    if (!new_path) {
        free(old_path);
        return;
    }

    if (rename_file(old_path, new_path) == 0) {
        printf(ANSI_COLOR_GREEN "✓ Renamed '%s' → '%s'\n" ANSI_COLOR_RESET, old_path, new_path);
        log_operation("CLI: Renamed '%s' to '%s'", old_path, new_path);
    } else {
        printf(ANSI_COLOR_RED "✗ Failed to rename '%s'\n" ANSI_COLOR_RESET, old_path);
        log_operation("CLI: Failed to rename '%s' to '%s'", old_path, new_path);
    }

    free(old_path);
    free(new_path);
}

/* ── Compress file operation ────────────────────────────────– */
static void cli_compress_file(void)
{
    printf(ANSI_COLOR_MAGENTA "\n--- Compress File ---\n" ANSI_COLOR_RESET);

    char *source = cli_get_filepath("Enter source file path: ");
    if (!source) {
        return;
    }

    int val = cli_validate_file(source);
    if (val != CLI_SUCCESS) {
        free(source);
        return;
    }

    char *dest = cli_get_filepath("Enter destination path: ");
    if (!dest) {
        free(source);
        return;
    }

    printf(ANSI_COLOR_YELLOW "Compressing '%s' to '%s'...\n" ANSI_COLOR_RESET, source, dest);
    if (compress_file(source, dest) == 0) {
        printf(ANSI_COLOR_GREEN "✓ File compressed successfully\n" ANSI_COLOR_RESET);
        log_operation("CLI: Compressed '%s' to '%s'", source, dest);
    } else {
        printf(ANSI_COLOR_RED "✗ Compression failed\n" ANSI_COLOR_RESET);
        log_operation("CLI: Compression failed for '%s'", source);
    }

    free(source);
    free(dest);
}

/* ── Main CLI loop ────────────────────────────────────────────– */
void cli_run(void)
{
    while (1) {
        cli_show_menu();

        int choice = cli_get_choice();
        if (choice == -1) {
            continue;
        }

        switch (choice) {
            case 1:
                cli_delete_file();
                break;
            case 2:
                cli_view_metadata();
                break;
            case 3:
                cli_rename_file();
                break;
            case 4:
                cli_compress_file();
                break;
            case 5:
                printf(ANSI_COLOR_GREEN "\n✓ Goodbye!\n" ANSI_COLOR_RESET);
                log_operation("CLI: User exit");
                return;
            default:
                printf(ANSI_COLOR_RED "ERROR: Invalid choice\n" ANSI_COLOR_RESET);
        }
    }
}
