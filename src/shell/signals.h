#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

/**
 * Do nothing. Meant to be used as a signal handler in place of SIG_IGN so that
 * it gets reset to default when a child execs.
 *
 * @param[in] sig_num   Signal number
 */
void do_nothing(int sign_num);

/**
 * SIGINT custom handler. Print a new prompt on a new line. If there is an
 * error, exit with a failure status.
 *
 * While running a fg job, temporarily disable by calling ignore_sigint(). When
 * the job finishes, reenable by calling handle_sigint().
 *
 * @param[in] sig_num   Signal number
 */
void sigint(int sig_num);

/** SIGCHLD custom handler. Update the status of the child process in the job
 * list and reap it if it's a zombie.
 *
 * @param[in] sig_num   Signal number
 */
void sigchld(int sig_num);

/**
 * Block SIGCHLD to prevent race conditions
 */
void block_sigchld(void);

/**
 * Unblock SIGCHLD and wait for a signal, then reblock it. Returns when a
 * signal comes.
 */
void wait_sigchld(void);

/**
 * Unblock SIGCHLD
 */
void unblock_sigchld(void);

/**
 * Set up signal handlers. If there is an error, display an error message and
 * exit with a failure status.
 *
 * Noop if not running interactively.
 */
void setup_signals(void);

volatile bool prompted;
#endif
