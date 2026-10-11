#include "job_list.h"
#include "user_interaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
/**
 * Allocate a JobEntry for a pipeline with the given number of processes. On
 * malloc failure, exit with a failure status.
 *
 * @param[in] job_str   Job pipeline string. This function takes ownership of
 *                      the memory allocation it uses.
 * @param[in] num_procs Number of processes in this job
 *
 * @return A pointer to the allocated JobEntry.
 */
JobEntry *create_job_entry(char *job_str, int num_procs) {
  JobEntry *job = malloc(sizeof(JobEntry) + num_procs * sizeof(ProcEntry));
  if (job == NULL) {
    perr_fatal("malloc");
  }

  job->num = -1;
  job->pgid = -1;
  job->cmd_str = job_str;
  job->status = JOB_RUNNING;
  job->status_change = false;
  job->num_procs = num_procs;
  job->next = NULL;
  job->prev = NULL;
  job->next_st = NULL;
  job->prev_st = NULL;

  for (int i = 0; i < num_procs; i++) {
    job->procs[i].pid = -1;
    job->procs[i].status = PROC_RUNNING;
    job->procs[i].signaled = false;
  }

  return job;
}

/**
 * Add the job entry to the job list and assign it the next job number.
 *
 * @param[in] jobs      Pointer to the job list
 * @param[in] job_entry Pointer to the job entry to add
 *
 * @return The assigned job number. It will already be stored in the JobEntry.
 */
int add_job_to_list(JobList *jobs, JobEntry *entry) {
  if (jobs->head == NULL) {
    entry->num = 1;
    jobs->head = entry;
    jobs->tail = entry;
    return entry->num;
  }

  entry->num = jobs->tail->num + 1;
  jobs->tail->next = entry;
  entry->prev = jobs->tail;
  jobs->tail = entry;
  return entry->num;
}

/**
 * Fetch a job from the job list by job number.
 *
 * @param[in] jobs      The job list
 * @param[in] job_num   Job number to fetch
 *
 * @return Pointer to the fetched job entry, or NULL if not found
 */
JobEntry *get_job_by_id(JobList *jobs, int job_num) {
  for (JobEntry *entry = jobs->head; entry != NULL; entry = entry->next) {
    if (entry->num == job_num) {
      return entry;
    }
  }
  return NULL;
}

ProcEntry *get_proc_by_pid(JobEntry *entry, pid_t pid) {
  for (size_t i = 0; i < entry->num_procs; i++) {
    if (entry->procs[i].pid == pid) {
      return &entry->procs[i];
    }
  }

  return NULL;
}

/**
 * Fetch a process entry and its owning job by PID.
 *
 * @param[in]  jobs       The job list
 * @param[in]  pid        Job pid.
 * @param[out] proc_entry Pointer to matched process entry. Invalid if return
 *                        is NULL.
 *
 * @return Pointer to the fetched job entry, or NULL if not found
 */
JobEntry *get_job_by_pid(JobList *jobs, pid_t pid, ProcEntry **proc_entry) {
  for (JobEntry *entry = jobs->head; entry != NULL; entry = entry->next) {
    if ((*proc_entry = get_proc_by_pid(entry, pid)) != NULL) {
      return entry;
    }
  }

  return NULL;
}

/**
 * Iterate over the jobs in a job list in ascending order. Use like this:
 *
 * for (JobEntry *job = next_job(jobs); job != NULL; job = next_job(jobs))
 *
 * @param[in] jobs  JobList to start iterating over. Set to NULL in subsequent
 *                  calls to continue iterating.
 *
 * @return The next JobEntry in the list, or NULL if there are no more.
 */
JobEntry *next_job(JobList *jobs) {
  static JobEntry *entry;
  if (jobs != NULL) {
    entry = jobs->head;
  } else {
    entry = entry->next;
  }

  return entry;
}

/**
 * Remove a job from the stopped stack, if present.
 *
 * @param[in] jobs  The job list
 * @param[in] entry The JobEntry to remove
 */
void remove_from_stopped(JobList *jobs, JobEntry *entry) {
  if (entry->next_st != NULL) {
    entry->next_st->prev_st = entry->prev_st;
  }
  if (jobs->stopped == entry) {
    jobs->stopped = entry->next_st;
  } else if (entry->prev_st != NULL) {
    entry->prev_st->next_st = entry->next_st;
  }
}

/**
 * Get the most recently created background job
 *
 * @param[in] jobs  The job list
 *
 * @return The desired JobEntry, or NULL if there is none
 */
JobEntry * get_bg(JobList *jobs) {
  for (JobEntry *job = jobs->tail; job != NULL; job = job->prev) {
    if (job->status == JOB_RUNNING) {
      return job;
    }
  }
  return NULL;
}

/**
 * Push this job to onto the stopped stack. If already on the bg or stopped
 * stack, it is removed first.
 *
 * @param[in] jobs  The job list
 * @param[in] entry The JobEntry to push
 */
void push_stopped(JobList *jobs, JobEntry *entry) {
  remove_from_stopped(jobs, entry);
  if (jobs->stopped != NULL) {
    entry->next_st = jobs->stopped;
    jobs->stopped->prev_st = entry;
  }
  jobs->stopped = entry;
}

/**
 * Get the top job from the stopped stack
 *
 * @param[in] jobs  The job list
 *
 * @return The JobEntry that was on top, or NULL if there is none
 */
JobEntry * get_stopped(JobList *jobs) {
  return jobs->stopped;
}

/**
 * Free the memory used by this job entry and remove it from the job list. If
 * it was not yet in the job list, just free the memory.
 *
 * @param[in,out] head  Pointer to the job list. If the job was never added to
 *                      the list, this parameter is ignored and can be NULL.
 * @param[in] entry     Pointer to the JobEntry to delete
 */
void delete_job_entry(JobList *jobs, JobEntry *entry) {
  if (entry->num > 0) {
    if (entry->prev == NULL) {
      jobs->head = entry->next;
    } else {
      entry->prev->next = entry->next;
    }
    if (entry->next == NULL) {
      jobs->tail = entry->prev;
    } else {
      entry->next->prev = entry->prev;
    }
  }

  remove_from_stopped(jobs, entry);

  free(entry->cmd_str);
  free(entry);
}
