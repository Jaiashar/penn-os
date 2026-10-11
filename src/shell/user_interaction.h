#ifndef USER_INTERACTION_H
#define USER_INTERACTION_H

#include <stdbool.h>
#include <stdnoreturn.h>

/**
 * Set to true if running interactively
 */
extern bool interactive;

/**
 * Maximum length of an input line
 */
enum {MAX_LINE_LENGTH = 4095};

/**
 * Stores the details of a parsed job, returned by get_next_job(). Once no
 * longer needed, contents must be cleaned up with free_job_info().
 */
typedef struct JobInfo {
    char *job_string;   /**< Job string in standardized form, e.g.
                             "cmd1 < input | cmd2 | cmd3 | cmd4 > output" */
    char ***commands;   /**< array of pipeline command arrays, each ready to be
                             passed to execvp(3) */
    int command_count;  /**< Number of commands in the pipeline */
    char *stdin;        /**< Name of file to read as stdin of first commend, or
                             NULL for no redirection */
    char *stdout;       /**< Name of file to write as stdout of last commend,
                             or NULL for no redirection */
    bool append;        /**< True if stdout file should be appended, false if
                             it should be truncated */
    bool background;    /**< Whether the job should run in the background */
} JobInfo;

/**
 * Writes a message denoting a fatal error message to STDERR and exits
 * unsuccessfully.
 *
 * @param[in] msg Error message to write.
 */
noreturn void err_fatal(const char *msg);

/**
 * Writes a message denoting a fatal error message to STDERR and exits
 * unsuccessfully. Uses perror to print error details.
 *
 * @param[in] msg Error message to write.
 */
noreturn void perr_fatal(const char *msg);


/**
 * Write the given string to the given file descriptor with error checking.
 * If it fails, display an error and exit with a failure status.
 *
 * @param[in] fd    File descriptor to write to.
 * @param[in] msg   The message to write.
 */
void write_check(int fd, const char *msg);

void write_check_format(int fd, const char *template, ...);
/**
 * Write the given string to the given file descriptor with error checking.
 * If it fails, display an error and exit with a failure status. Safe for use
 * in signals, but error message is less descriptive.
 *
 * @param[in] fd    File descriptor to write to.
 * @param[in] msg   The message to write.
 */
void write_sigsafe(int fd, const char *msg);

/**
 * Check if stdin is a TTY or not. If there is an error, exit the program.
 *
 * @return true if stdin is a TTY, false otherwise.
 */
bool is_interactive(void);

/**
 * Prompt for and read a job, handling errors and printing a newline if one was
 * not typed. If there is an error, exit with a failure status.
 *
 * If the input was empty or invalid, keep retrying.
 *
 * If the user presses Ctrl-D on an empty line, exit the program.
 *
 * @return A JobInfo struct with all the parsed details of the job. Be sure to
 *         call free_job_info() when it is no longer needed.
 */

bool try_prompt_job(JobInfo *job_info);
/**
 * Free the contents of a JobInfo when it is no longer needed.
 *
 * @param[in] job_info  The JobInfo to free
 */
void free_job_info(JobInfo job_info);

#endif
