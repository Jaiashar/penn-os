#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <stdbool.h>
#include <sys/types.h>
#include "user_interaction.h"
#include "job_list.h"

/** True if the --async option was given, false otherwise */
extern bool async;

/** The global job list */
extern JobList jobs;
/** The foreground job entry, or NULL */
extern JobEntry *fg_entry;

/**
 * Print status changes and clean up finished jobs
 */
void handle_status_changes();

/**
 * Updates a process/ProcEntry status based on waitpid(2) WSTATUS
 * result.
 *
 * @param[in] entry The process entry to update.
 * @param[in] wstatus The WSTATUS return from waitpid(2).
 */
void update_process_status(ProcEntry *entry, int wstatus);

/**
 * Reconciles the status of a single job with the statuses
 * of its component processes.
 *
 * @param[in] entry The job to reconcile.
 */
void reconcile_single_job(JobEntry *entry);

/**
 * Checks for status changes in all child processes and compares
 * any changes with jobs in the job list.
 *
 * All job entries in the list are updated with the new information.
 *
 * @param[in] from_sigchld  Set to true if calling from the SIGCHLD handler.
 *                          This makes it check fg_entry as well.
 */
void update_job_statuses(bool from_sigchld);

/**
 * Takes in a job definition and forks into a child process
 * that tries to execute the command in the job definition.
 *
 * If the process is a background process, this method adds
 * it to the jobs list and returns control to the caller.
 *
 * @param[in] job_info The job definition.
 */
void submit_job(JobInfo job_info);

/**
 * Give a job terminal control and wait for it to either finish or be stopped.
 * If the job was not already running, continue it first.
 *
 * If there are errors, exits with a failure status.
 *
 * @param[in] entry The JobEntry to move to the foreground.
 */
void run_in_fg(JobEntry *entry);

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
void run_command(JobInfo job_info);

#endif
