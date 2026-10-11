#ifndef JOB_LIST_H
#define JOB_LIST_H
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

/**
 * Process running statuses
 */
typedef enum ProcStatus {
    PROC_RUNNING,   /**< Process is started or continued */
    PROC_STOPPED,   /**< Process is stopped */
    PROC_FINISHED   /**< Process exited or terminated by signal */
} ProcStatus;

/**
 * Job statuses
 */
typedef enum JobStatus {
    JOB_STOPPED,    /**< All processes in this job have stopped (or died) */
    JOB_RUNNING,    /**< All processes in this job have continued (or died) */
    JOB_FINISHED    /**< All processes in this job have finished */
} JobStatus;

/**
 * Process entry - Stores the status of individual processes within a job
 */
typedef struct ProcEntry {
    pid_t pid;          /**< PID of this process */
    ProcStatus status;  /**< Current running status of this process */
    bool signaled;      /**< True if this process was terminated by signal */
} ProcEntry;

/**
 * Job list entry
 */
typedef struct JobEntry {
    int num;                /**< Job number, or -1 if a newly started fg job */
    pid_t pgid;             /**< Job's process group ID */
    char *cmd_str;          /**< Pipeline command string */
    JobStatus status;       /**< Status of the job overall */
    bool status_change;     /**< True if the status changed and should print */
    size_t num_procs;       /**< Number of processes in this job's pipeline */
    struct JobEntry *prev;  /**< Prev entry in list, or NULL if the head */
    struct JobEntry *next;  /**< Next entry in list, or NULL if the tail */
    struct JobEntry *next_st;  /**< Next entry in the stopped stack, or NULL
                                    if the end of the stack */
    struct JobEntry *prev_st;  /**< Prev entry in the stopped stack, or NULL
                                    if the start of the stack */
    ProcEntry procs[];      /**< Flexible array member of process statuses */
} JobEntry;

/**
 * Job list. Entries are kept in ascending order from head to tail.
 */
typedef struct JobList {
    JobEntry *head;     /**< Pointer to head of the job list */
    JobEntry *tail;     /**< Pointer to tail of the job list */
    JobEntry *stopped;  /**< Pointer to the head of the stopped stack - keeps
                             track of most recently stopped jobs */
} JobList;

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
JobEntry * create_job_entry(char *job_str, int num_procs);

/**
 * Add the job entry to the job list and assign it the next job number.
 *
 * @param[in] jobs      Pointer to the job list
 * @param[in] job_entry Pointer to the job entry to add
 *
 * @return The assigned job number. It will already be stored in the JobEntry.
 */
int add_job_to_list(JobList *jobs, JobEntry *entry);

/**
 * Fetch a job from the job list by job number.
 *
 * @param[in] jobs      The job list
 * @param[in] job_num   Job number to fetch
 *
 * @return Pointer to the fetched job entry, or NULL if not found
 */
JobEntry * get_job_by_id(JobList *jobs, int job_num);


ProcEntry *get_proc_by_pid(JobEntry *entry, pid_t pid);

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
JobEntry *get_job_by_pid(JobList *jobs, pid_t pid, ProcEntry **proc_entry);

/**
 * Iterate over the jobs in a job list in ascending order. Use like this:
 *
 * for (JobEntry *job = next_job(jobs); job != NULL; job = next_job(NULL))
 *
 * @param[in] jobs  JobList to start iterating over. Set to NULL in subsequent
 *                  calls to continue iterating.
 *
 * @return The next JobEntry in the list, or NULL if there are no more.
 */
JobEntry *next_job(JobList *jobs);

/**
 * Get the most recently created background job
 *
 * @param[in] jobs  The job list
 *
 * @return The desired JobEntry, or NULL if there is none
 */
JobEntry * get_bg(JobList *jobs);

/**
 * Push this job to onto the stopped stack. If already on the bg or stopped
 * stack, it is removed first.
 *
 * @param[in] jobs  The job list
 * @param[in] entry The JobEntry to push
 */
void push_stopped(JobList *jobs, JobEntry *entry);

/**
 * Get the top job from the stopped stack
 *
 * @param[in] jobs  The job list
 *
 * @return The JobEntry that was on top, or NULL if there is none
 */
JobEntry * get_stopped(JobList *jobs);

/**
 * Remove a job from the stopped stack, if present.
 *
 * @param[in] jobs  The job list
 * @param[in] entry The JobEntry to remove
 */
void remove_from_stopped(JobList *jobs, JobEntry *entry);

/**
 * Free the memory used by this job entry and remove it from the job list. If
 * it was not yet in the job list, just free the memory.
 *
 * @param[in,out] head  Pointer to the job list. If the job was never added to
 *                      the list, this parameter is ignored and can be NULL.
 * @param[in] entry     Pointer to the JobEntry to delete
 */
void delete_job_entry(JobList *jobs, JobEntry *entry);

#endif
