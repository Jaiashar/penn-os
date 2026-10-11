/** @file
 * Defines builtin routines as needed for the shell.
 */
#ifndef SHELL_BUILTINS_H
#define SHELL_BUILTINS_H
#include <sys/types.h>

/**
 * Similar to Unix's cat(1).
 * Runs like:
 * ```
 * cat FILE1 FILE2 ... FILEN to concatenate the contents
 * of files 1...N and write to STDOUT.
 * ```
 * When no arguments are given, reads from STDIN.
 *
 * @param argv NULL-terminated list of arguments.
 */
void s_cat(char **argv);

/**
 * Similar to Unix's sleep(1).
 * Takes in a single argument that determines the
 * time to wait in seconds. This time is a minimum time
 * waited and there is no guarantee it won't wait longer.
 * @param argv NULL-terminated list of arguments.
 */
void s_sleep(char **argv);

/**
 * This command busy-waits indefinitely. Takes no arguments.
 * @param argv NULL-terminated list of arguments.
 */
void s_busy(char **argv);

/**
 * Similar to Unix's echo(1). Takes in N arguments and writes
 * them, space-delimited, to STDOUT.
 * @param argv NULL-terminated list of arguments.
 */
void s_echo(char **argv);

/**
 * Similar to ls(1). Outputs the contents of the
 * current directory or if run like `ls <dir>` outputs the
 * contents of the directory `dir`.
 * @param argv NULL-terminated list of arguments.
 */
void s_ls(char **argv);

/**
 * Similar to touch(1). Takes in a list of file as
 * arguments, and either updates the timestamps of each if
 * they exist or creates empty files if they do not.
 * @param argv NULL-terminated list of arguments.
 */
void s_touch(char **argv);

/**
 * Similar to mv(1). Run like
 * ```
 * mv <old> <new>
 * ```
 * Unlinks the file <old> and creates a new link at <new>
 * that points to the existing data.
 * @param argv NULL-terminated list of arguments.
 */
void s_mv(char **argv);

/**
 * Similar to cp(1). Run like
 * ```
 * cp <file> <copy>
 * ```
 * Creates a new file link "copy" that points to a copy of the data
 * pointed to by "file".
 * @param argv NULL-terminated list of arguments.
 */
void s_cp(char **argv);

/**
 * Similar to rm(1).
 * ```
 *  rm FILE1 FILE2 ... FILEN
 * ```
 * Removes all files listed as arguments.
 * @param argv NULL-terminated list of arguments.
 */
void s_rm(char **argv);

/**
 * Similar to chmod(1). Run like
 * ````
 * chmod [+wx] [-wx] FILE
 * ````
 * Adds or removes permisions from FILE based on the
 * format provided.
 * @param argv NULL-terminated list of arguments.
 */
void s_chmod(char **argv);

/**
 * This is a shell subroutine:
 * ```
 * nice <priority> <cmd>
 * ```
 * which runs the other builtin given by "cmd" with a priotity given by "priority".
 * @param argv NULL-terminated list of arguments.
 */
pid_t s_nice (char **argv, int in_fd, int out_fd);

/**
 * Similar to ps(1), prints out all running processes.
 * @param argv NULL-terminated list of arguments.
 */
void s_ps(char **argv);

/**
 * Loosely based on kill(1):
 * ```
 * kill [-term | -stop | -cont] <pid>.
 * ```
 * Sends a signal to the process at PID and either kills, stops, or continues.
 * @param argv NULL-terminated list of arguments.
 */
void s_kill(char **argv);

/**
 * Calls the "zombify" stress test.
 * @param argv NULL-terminated list of arguments.
 */
void zombify(char **argv);

/**
 * Calls the "orphanify" stress test.
 * @param argv NULL-terminated list of arguments.
 */
void orphanify(char **argv);

/**
 * Spawn the specified command+arguments
 *
 * @param argv      Command and arguments to run
 * @param in_fd     File descriptor to use as stdin
 * @param out_fd    File descriptor to use as stdout
 *
 * @return The PID of the running process, or -1 on error
 */
int run_builtin(char **argv, int in_fd, int out_fd);
#endif
