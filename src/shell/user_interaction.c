#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdnoreturn.h>
#include <stdarg.h>

#include "parsejob.h"
#include "user_interaction.h"
#include "../os/process.h"
#include "../os/pennfat.h"
#include "../os/p_errno.h"
#include "../kernel/logger.h"
#include "signals.h"

/**
 * Set to true if running interactively
 */
bool interactive = false;

static const char WRITE_ERR_MSG[] = "write() failed in signal";

/** @internal
 * Write the given string to the given file descriptor with error checking.
 * If it fails, exit with a failure status. Optionally avoid async-unsafe
 * calls.
 *
 * @param[in] fd        File descriptor to write to.
 * @param[in] msg       The message to write.
 * @param[in] sigsafe   Whether only async-safe calls should be used.
 */
static void write_check_base(int fd, const char *msg, bool sigsafe) {
    size_t len = strlen(msg);
    size_t written = 0;
    while (written < len) {
        ssize_t result = f_write(fd, msg + written, len - written);
        if (result < 0) {
            if (sigsafe) {
                // perror isn't async-safe. Best effort attempt to write an
                // error message (might succeed if fd was redirected away from
                // the console)
                f_write(F_STDOUT_NO, WRITE_ERR_MSG, strlen(WRITE_ERR_MSG));
            } else {
                p_perror("write_check_f_read");
            }

            p_exit();
        }
        written += result;
    }
}

/**
 * Writes a message denoting a fatal error message to STDERR and exits
 * unsuccessfully.
 *
 * @param[in] msg Error message to write to perror(3).
 */
noreturn void err_fatal(const char *msg) {
    p_perror(msg);
    p_exit();
    exit(0);
}

/**
 * Writes a message denoting a fatal error message to STDERR and exits
 * unsuccessfully. Uses perror to print error details.
 *
 * @param[in] msg Error message to write to perror(3).
 */
noreturn void perr_fatal(const char *msg) {
  p_perror(msg);
  p_exit();
  exit(0);
}


/**
 * Write the given string to the given file descriptor with error checking.
 * If it fails, display an error and exit with a failure status.
 *
 * @param[in] fd    File descriptor to write to.
 * @param[in] msg   The message to write.
 */
void write_check(int fd, const char *msg) {
    write_check_base(fd, msg, false);
}

void write_check_format(int fd, const char *template, ...) {
    va_list args;
    va_start(args, template);
    size_t n = vsnprintf(NULL, 0, template, args);
    va_end(args);

    if (n == 0) {
        return;
    }

    va_start(args, template);
    char buffer[n];
    vsprintf(buffer, template, args);
    va_end(args);

    write_check(fd, buffer);
}

/**
 * Write the given string to the given file descriptor with error checking.
 * If it fails, display an error and exit with a failure status. Safe for use
 * in signals, but error message is less descriptive.
 *
 * @param[in] fd    File descriptor to write to.
 * @param[in] msg   The message to write.
 */
void write_sigsafe(int fd, const char *msg) {
    write_check_base(fd, msg, true);
}

/**
 * Check if stdin is a TTY or not. If there is an error, exit the program.
 *
 * @return true if stdin is a TTY, false otherwise.
 */
bool is_interactive(void) {
    errno = 0;
    if (isatty(STDIN_FILENO)) {
        return true;
    }
    if (errno == ENOTTY) {
        return false;
    }
    err_fatal("isatty");
}

/** @internal
 * Read and parse a job, handling errors and printing a newline if one was not
 * typed. If there is an error, exit with a failure status.
 *
 * If the user presses Ctrl-D on an empty line, exit the program.
 *
 * @param[out] job_info     A pointer to a JobInfo struct to fill with the
 *                          parsed details of the job. If the input was empty
 *                          or invalid, it will be untouched.
 *
 * @return true if the input was valid and nonempty, false otherwise
 */
bool try_prompt_job(JobInfo *job_info) {
    ssize_t result;
    size_t n = MAX_LINE_LENGTH + 1;
    char *cmd = malloc(n);
    if (cmd == NULL) {
        err_fatal("malloc");
    }

    // Read the raw job string from stdin
    if (interactive) {
        if(!prompted)
            write_check(F_STDOUT_NO, PROMPT);
        prompted = false;
        result = f_read(F_STDIN_NO, MAX_LINE_LENGTH, cmd);
        if (result == 0) {
            // Ctrl-D on an empty line
            write_check(F_STDOUT_NO, "\n");
            free(cmd);
            p_exit();
        } else if (result < 0) {
            // Fail
            p_perror("read");
            free(cmd);
            exit(EXIT_FAILURE);
        }
        if (cmd[result - 1] != '\n') {
            // User submitted with Ctrl-D
            write_check(F_STDOUT_NO, "\n");
        }
        cmd[result] = '\0';
    } else {
        result = getline(&cmd, &n, stdin);
        if (result < 0) {
            if (feof(stdin)) {
                free(cmd);
                exit(EXIT_SUCCESS);
            }
            p_perror("getline");
            free(cmd);
            exit(EXIT_FAILURE);
        }
    }

    // Do the actual parsing
    if (!parseJob(cmd, interactive)) {
        free(cmd);
        return false;
    }
    free(cmd);

    job_info->job_string = getJobPipeline();
    job_info->commands = getJobCommands();
    job_info->command_count = getCommandCount();
    job_info->stdin = getJobStdin();
    job_info->stdout = getJobStdout();
    job_info->append = isAppendingStdout();
    job_info->background = isBackgroundJob();
    return true;
}


/**
 * Free the contents of a JobInfo when it is no longer needed.
 *
 * @param[in] job_info  The JobInfo to free
 */
void free_job_info(JobInfo job_info) {
    freeJobCommands(job_info.commands);
    free(job_info.stdin);
    free(job_info.stdout);
}
