#include "job_list.h"
#include "job_control.h"
#include "parsejob.h"
#include "signals.h"
#include "user_interaction.h"
#include "../os/process.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include "../os/p_errno.h"
#include "../os/shell_builtins.h"
#include "../os/types.h"
#include "../os/pennfat.h"

/** True if the --async option was given, false otherwise */
bool async = false;

/** The global job list */
JobList jobs = {
  .head = NULL,
  .tail = NULL,
  .stopped = NULL
};
/** The foreground job entry, or NULL */
JobEntry *fg_entry = NULL;

/**
 * Pretty-prints the provided job (sub)-process status to STDERR, like:
 * ```
 *   [<job_id>]    <command_line> (<status>)
 * ```
 *
 * @param[in] entry The job process to print.
 */
static void print_job_status(JobEntry *entry) {
  switch (entry->status) {
  case JOB_STOPPED:
    write_check_format(F_STDOUT_NO, "[%d] \t%s (stopped)\n", entry->num, entry->cmd_str);
    break;

  case JOB_RUNNING:
    write_check_format(F_STDOUT_NO, "[%d] \t%s (running)\n", entry->num, entry->cmd_str);
    break;

  case JOB_FINISHED:
    // Finished jobs will be cleaned before the next prompt and shouldn't print
    // in the "jobs" output
    break;

  default:
    err_fatal("print_job_status");
    break;
  }
}

/**
 * Pretty-prints the provided job (sub)-process status to STDERR, like:
 * ```
 *   <status>: <command_line>
 * ```
 *
 * @param[in] entry The job process to print.
 */
static void print_job_status_update(JobEntry *entry) {
  switch (entry->status) {
  case JOB_STOPPED:
    write_check_format(F_STDOUT_NO, "Stopped[%d]: %s\n", entry->num, entry->cmd_str);
    break;

  case JOB_RUNNING:
    write_check_format(F_STDOUT_NO, "Running[%d]: %s\n", entry->num, entry->cmd_str);
    break;

  case JOB_FINISHED:
    write_check_format(F_STDOUT_NO, "Finished[%d]: %s\n", entry->num, entry->cmd_str);
    break;

  default:
    err_fatal("print_job_status_update");
    break;
  }
}

/**
 * Updates a process/ProcEntry status based on waitpid(2) WSTATUS
 * result.
 *
 * @param[in] entry The process entry to update.
 * @param[in] wstatus The WSTATUS return from waitpid(2).
 */
void update_process_status(ProcEntry *entry, int wstatus) {
  if (W_WIFEXITED(wstatus)) {
    entry->status = PROC_FINISHED;
  } else if (W_WIFSIGNALED(wstatus)) {
    entry->status = PROC_FINISHED;
    entry->signaled = true;
  } else if (W_WIFSTOPPED(wstatus)) {
    entry->status = PROC_STOPPED;
  } else if (W_WIFCONTINUED(wstatus)) {
    entry->status = PROC_RUNNING;
  }
}

/**
 * Mark a job and all stopped processes within as running
 */
static void mark_job_running(JobEntry *entry) {
  for (size_t i = 0; i < entry->num_procs; i++) {
    if (entry->procs[i].status == PROC_STOPPED) {
      entry->procs[i].status = PROC_RUNNING;
    }
  }
  entry->status = JOB_RUNNING;
}

/**
 * Reconciles the status of a single job with the statuses
 * of its component processes.
 *
 * @param[in] entry The job to reconcile.
 */
void reconcile_single_job(JobEntry *entry) {
  size_t finished = 0, stopped = 0, running = 0;
  for (size_t i = 0; i < entry->num_procs; i++) {
    ProcStatus status = entry->procs[i].status;
    if (status == PROC_FINISHED) {
      finished++;
    } else if (status == PROC_STOPPED) {
      stopped++;
    } else if (status == PROC_RUNNING) {
      running++;
    }
  }

  // Set job status if it's in a consistent state.
  if (finished == entry->num_procs) {
    entry->status = JOB_FINISHED;
  } else if (running == 0 && stopped > 0) {
    entry->status = JOB_STOPPED;
  } else if (stopped == 0 && running > 0) {
    entry->status = JOB_RUNNING;
  } else if (stopped + running + finished != entry->num_procs) {
    // Bad application state. Force exit.
    err_fatal("reconcile_single_job");
  }
}

/**
 * Print status changes and clean up finished jobs
 */
void handle_status_changes() {
  JobEntry *entry = next_job(&jobs);
  while (entry != NULL) {
    if (entry->status_change) {
      entry->status_change = false;
      print_job_status_update(entry);
    }

    JobEntry *prev = entry;
    entry = next_job(NULL);
    if (prev->status == JOB_FINISHED) {
      delete_job_entry(&jobs, prev);
    }
  }
}

/**
 * For each job in the list, reconciles its overall
 * status with the status of each process in the job.
 *
 * If there is a change in job state, marks it for later printing.
 */
static void reconcile_job_statuses() {
  for (JobEntry *entry = next_job(&jobs);
       entry != NULL;
       entry = next_job(NULL)) {
    if (entry->num_procs == 0) {
      // Invalid state...
      err_fatal("sync_process_job_status");
    }

    JobStatus old = entry->status;
    reconcile_single_job(entry);

    // Only need to print status if it has changed.
    if (old != entry->status) {
      entry->status_change = true;
      if (entry->status == JOB_STOPPED) {
        push_stopped(&jobs, entry);
      } else if (entry->status == JOB_RUNNING) {
        remove_from_stopped(&jobs, entry);
      }
    }
  }
}

/**
 * Waits all members of a job to finish or STOP.
 *
 * @param[in] entry The job entry definition to wait on.
 * @return true if job is stopped and should be background-queued, and
 false if job finished.
*/
bool wait_for_job(JobEntry *entry) {
  int wstatus;
  int signaled = 0;
      //int flags = interactive ? WUNTRACED : 0;
      pid_t pid = p_waitpid(entry->procs[0].pid, &wstatus, false);

      if (pid != -1) {
        if (interactive) {
          ProcEntry *proc_entry = get_proc_by_pid(entry, pid);
          update_process_status(proc_entry, wstatus);

          reconcile_single_job(entry);
          if (entry->status == JOB_STOPPED) {
            return true;
          }
        }

        ;
      }

      else {
        if(p_errno() == KECHILD) return false;
        perr_fatal("wait_pid");
      }

  if (interactive && signaled) {
    write_check(F_STDOUT_NO, "\n");
  }

  return false;
}

/**
 * Checks for status changes in all child processes and compares
 * any changes with jobs in the job list.
 *
 * All job entries in the list are updated with the new information.
 *
 * @param[in] from_sigchld  Set to true if calling from the SIGCHLD handler.
 *                          This makes it check fg_entry as well.
 */
void update_job_statuses(bool from_sigchld) {
  int wstatus;
  pid_t pid;
  while ((pid = p_waitpid(-1, &wstatus, true)) > 0) {
    JobEntry *job_entry;
    ProcEntry *proc_entry;
    if (from_sigchld &&
        fg_entry != NULL &&
        (proc_entry = get_proc_by_pid(fg_entry, pid)) != NULL) {
      update_process_status(proc_entry, wstatus);
      continue;
    }
    if ((job_entry = get_job_by_pid(&jobs, pid, &proc_entry)) == NULL) {
      err_fatal("poll_job_status");
    }

    update_process_status(proc_entry, wstatus);
  }

  if (pid == -1 && p_errno() != KECHILD) {
    perr_fatal("waitpid(WNOHANG)");
  }

  reconcile_job_statuses();
  if (from_sigchld && fg_entry != NULL) {
    reconcile_single_job(fg_entry);
  }
}

/**
 * Takes in a job definition and forks into a child process
 * that tries to execute the command in the job definition.
 *
 * If the process is a background process, this method adds
 * it to the jobs list and returns control to the caller.
 *
 * @param[in] job_info The job definition.
 */
void submit_job(JobInfo job_info) {
  int count = job_info.command_count;
  if (count <= 0) {
    return;
  }

  if (count > 1) {
      write_check(F_STDOUT_NO, "Pipelines not supported!\n");
      free(job_info.job_string);
      return;
  }

  int in_fd = F_STDIN_NO;
  int out_fd = F_STDOUT_NO;
  if (job_info.stdin != NULL) {
      in_fd = f_open(job_info.stdin, F_READ);
      if (in_fd < 0) {
          p_perror("shell: f_open");
          free(job_info.job_string);
          return;
      }
  }
  if (job_info.stdout != NULL) {
      if (job_info.append) {
          out_fd = f_open(job_info.stdout, F_APPEND);
      } else {
          out_fd = f_open(job_info.stdout, F_WRITE);
      }
      if (out_fd < 0) {
          p_perror("shell: f_open");
          if (in_fd != F_STDIN_NO) {
              if (f_close(in_fd) < 0) {
                  p_perror("shell: f_close");
              }
          }
          free(job_info.job_string);
          return;
      }
  }

  JobEntry *entry = create_job_entry(job_info.job_string, count);
  fg_entry = entry;

  char **argv = job_info.commands[0];

  int pid = run_builtin(argv, in_fd, out_fd);
  if (in_fd != F_STDIN_NO) {
      if (f_close(in_fd) < 0) {
          p_perror("shell: f_close");
      }
  }
  if (out_fd != F_STDOUT_NO) {
      if (f_close(out_fd) < 0) {
          p_perror("shell: f_close");
      }
  }
  if (pid == -1) {
    free(job_info.job_string);
    free(entry);
    fg_entry = NULL;
    return;
  }

  entry->procs[0].pid = pid;
  if (job_info.background) {
      add_job_to_list(&jobs, entry);
      fg_entry = NULL;
      print_job_status_update(entry);
  } else {
    if (wait_for_job(entry)) {
      add_job_to_list(&jobs, entry);
      push_stopped(&jobs, entry);
      fg_entry = NULL;
      print_job_status_update(entry);
    } else {
      delete_job_entry(&jobs, entry);
      fg_entry = NULL;
    }
  }
}


/**
 * Give a job terminal control and wait for it to either finish or be stopped.
 * If the job was not already running, continue it first.
 *
 * If there are errors, exits with a failure status.
 *
 * @param[in] entry The JobEntry to move to the foreground.
 */
void run_in_fg(JobEntry *entry) {
  if (entry->status == JOB_STOPPED) {
    write_check_format(F_STDOUT_NO, "Restarting [%d]:  %s\n", entry->num, entry->cmd_str);
  } else {
    write_check_format(F_STDOUT_NO, "[%d] %s\n", entry->num, entry->cmd_str);
  }
  remove_from_stopped(&jobs, entry);

  // if (tcsetpgrp(STDIN_FILENO, entry->pgid) == -1) {
  //   perr_fatal("tcsetpgrp");
  // }
  fg_entry = entry;

  if (entry->status == JOB_STOPPED) {
    p_kill(entry->procs[0].pid, S_SIGCONT);
    mark_job_running(entry);
  }

  if (wait_for_job(entry)) {
    push_stopped(&jobs, entry);
    fg_entry = NULL;
    print_job_status_update(entry);
  } else {
    delete_job_entry(&jobs, entry);
    fg_entry = NULL;
  }

  // if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
  //   perr_fatal("tcsetpgrp");
  // }
}

/** @internal
 * Get the JobEntry with id from the argument if given, or the current job
 * otherwise. Return NULL if there is no such job.
 *
 * @param[in] arg   The argument to fg or bg, or NULL if none
 */
static JobEntry * get_arg_or_current_job(const char *arg) {
  JobEntry *entry;
  if (arg != NULL) {
    int id = atoi(arg);
    if (id == 0) {
      return NULL;
    }
    entry = get_job_by_id(&jobs, id);
    if (entry == NULL) {
      return NULL;
    }
    if (entry->status == JOB_FINISHED) {
      return NULL;
    }
    return entry;
  }

  entry = get_stopped(&jobs);
  if (entry != NULL) {
    return entry;
  }
  entry = get_bg(&jobs);
  return entry;
}

/**
 * Run a builtin or submit a job. Returns control when any of:
 * 1. It was a new job and it finishes or is stopped
 * 2. It was a fg command and the the existing job finishes or is stopped
 * 3. Right away if it was a bg command
 * 4. After printing the job status if it was a jobs command
 *
 * If there is an error, exits with a failure status.
 *
 * @param[in] job_info  The command info
 */
void run_command(JobInfo job_info) {
  block_sigchld();
  char **argv = job_info.commands[0];
  if (strcmp(argv[0], "jobs") == 0) {
    if (interactive) {
      for (JobEntry *entry = next_job(&jobs);
           entry != NULL;
           entry = next_job(NULL)) {
        print_job_status(entry);
      }
    }
    free(job_info.job_string);
  } else if (strcmp(argv[0], "fg") == 0) {
    if (interactive) {
      JobEntry *entry = get_arg_or_current_job(argv[1]);
      if (entry == NULL) {
        write_check(F_STDOUT_NO, "fg: No such job\n");
      } else {
        run_in_fg(entry);
      }
    }
    free(job_info.job_string);
  } else if (strcmp(argv[0], "bg") == 0) {
    if (interactive) {
      JobEntry *entry = get_arg_or_current_job(argv[1]);
      if (entry == NULL) {
        write_check(F_STDOUT_NO, "bg: No such job\n");
      } else if (entry->status == JOB_RUNNING) {
        write_check(F_STDOUT_NO, "bg: Job already running\n");
      } else {
        p_kill(entry->procs[0].pid, S_SIGCONT);
        mark_job_running(entry);
        remove_from_stopped(&jobs, entry);
        print_job_status_update(entry);
      }
    }
    free(job_info.job_string);
  } else {
    submit_job(job_info);
    unblock_sigchld();
    return;
  }

  // submit_job() ensures the job_info job_string is freed (because it's freed
  // as part of a JobEntry). We have to do it now for builtins.
  
  unblock_sigchld();
}


/**
 * returns true if the job with ID pid is the foreground process
 * 
 * For use by shell builtins so that backrground processes that attempt to 
 * read from STDIN can be sent S_SIGSTOP
*/
bool is_job_fg(pid_t pid) {
  return fg_entry->procs[0].pid == pid;
}
