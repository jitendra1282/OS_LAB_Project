#ifndef CLI_H
#define CLI_H

/* ANSI color codes for terminal output */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Error codes */
#define CLI_SUCCESS        0
#define CLI_INVALID_INPUT  1
#define CLI_FILE_ERROR     2
#define CLI_PERMISSION_ERROR 3
#define CLI_NOT_FOUND      4

/**
 * Main CLI loop — interactive menu
 */
void cli_run(void);

/**
 * Display the main menu
 */
void cli_show_menu(void);

/**
 * Get and validate user choice
 * Returns the choice (1-5) or -1 on invalid input
 */
int cli_get_choice(void);

/**
 * Input validation — checks if file path exists and is readable
 * Returns 0 on success, error code on failure
 */
int cli_validate_file(const char *filepath);

/**
 * Get file path input from user with validation
 * Returns allocated string or NULL on error
 */
char *cli_get_filepath(const char *prompt);

#endif
