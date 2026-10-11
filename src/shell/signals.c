#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdbool.h>

#include "user_interaction.h"
#include "job_list.h"
#include "job_control.h"
#include "signals.h"
#include "../os/process.h"
#include "../os/types.h"

/**
 * SIGINT custom handler. Print a new prompt on a new line. If there is an
 * error, exit with a failure status.
 *
 * While running a fg job, temporarily disable by calling ignore_sigint(). When
 * the job finishes, reenable by calling handle_sigint().
 *
 * @param[in] sig_num   Signal number
 */
void sigint(int sig_num) {
    int save_errno = errno;
    write_sigsafe(F_STDOUT_NO, "\n");
    write_sigsafe(F_STDOUT_NO, PROMPT);
    prompted = true;
    if(fg_entry != NULL) {
      p_kill(fg_entry->procs[0].pid, S_SIGTERM);
    }
    errno = save_errno;
}

void sigstop(int sig_num) {
    int save_errno = errno;
    write_sigsafe(F_STDOUT_NO, "\n");
    if (fg_entry != NULL) {
        p_kill(fg_entry->procs[0].pid, S_SIGSTOP);
    }
    errno = save_errno;
}

void sigcont(int sig_num) {
    int save_errno = errno;
    if (fg_entry != NULL) {
        p_kill(fg_entry->procs[0].pid, S_SIGCONT);
    }
    errno = save_errno;
}

/**
 * Set up signal handlers. If there is an error, display an error message and
 * exit with a failure status.
 *
 * Noop if not running interactively.
 */
void setup_signals(void) {
  if (!interactive) {
    return;
  }
  if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
    perr_fatal("signal(SIGTTOU, SIG_IGN)");
  }
  if (signal(SIGTTIN, do_nothing) == SIG_ERR) {
    perr_fatal("signal(SIGTTIN)");
  }
  if (signal(SIGTSTP, sigstop) == SIG_ERR) {
    perr_fatal("signal(SIGTSTP)");
  }
  if (signal(SIGQUIT, do_nothing) == SIG_ERR) {
    perr_fatal("signal(SIGQUIT)");
  }
  if (signal(SIGINT, sigint) == SIG_ERR) {
    perr_fatal("signal(SIGINT)");
  }
  if (signal(SIGCONT, sigcont) == SIG_ERR) {
    perr_fatal("signal(SIGCONT)");
  }
  if (async) {
    if (signal(SIGCHLD, sigchld) == SIG_ERR) {
      perr_fatal("signal(SIGCHLD)");
    }
  }
}


/**
 * Do nothing. Meant to be used as a signal handler in place of SIG_IGN so that
 * it gets reset to default when a child execs.
 *
 * @param[in] sig_num   Signal number
 */
void do_nothing(int sign_num) {
}

/** SIGCHLD custom handler. Update the status of the child process in the job
 * list and reap it if it's a zombie.
 *
 * @param[in] sig_num   Signal number
 */
void sigchld(int sig_num) {
  int save_errno = errno;
  update_job_statuses(true);
  errno = save_errno;
}

/**
 * Block SIGCHLD to prevent race conditions
 */
void block_sigchld(void) {
  sigset_t sigset;
  if (sigemptyset(&sigset) < 0) {
    perr_fatal("sigemptyset");
  }
  if (sigaddset(&sigset, SIGCHLD) < 0) {
    perr_fatal("sigaddset(SIGCHLD)");
  }
  if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
    perr_fatal("sigprocmask(block SIGCHLD)");
  }
}

/**
 * Unblock SIGCHLD and wait for a signal, then reblock it. Returns when a
 * signal comes.
 */
void wait_sigchld(void) {
  sigset_t sigset;
  if (sigemptyset(&sigset) < 0) {
    perr_fatal("sigemptyset");
  }
  sigsuspend(&sigset);
  if (errno != EINTR) {
    perr_fatal("sigsuspend");
  }
}

/**
 * Unblock SIGCHLD
 */
void unblock_sigchld(void) {
  sigset_t sigset;
  if (sigemptyset(&sigset) < 0) {
    perr_fatal("sigemptyset");
  }
  if (sigaddset(&sigset, SIGCHLD) < 0) {
    perr_fatal("sigaddset(SIGCHLD)");
  }
  if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
    perr_fatal("sigprocmask(block SIGCHLD)");
  }
}
