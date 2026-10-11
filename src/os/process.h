/** @file
 * User-level API for process- and scheduler-related functions
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>
#include <sys/types.h>
#include "types.h"

/**
 * Spawns a new process given by the function pointer, inheriting
 * its file table from the calling process.
 * @param[in] func  The function defining the new process.
 * @param[in] argv  A NULL-terminated array of argments to be passed
 *                  to the function pointer when called.
 * @param[in] fd0   The file-descriptor to use for F_STDIN in the spawned
 *                  process.
 * @param[in] fd1   The file-descriptor to use for F_STDOUT in the spawned
 *                  process.
 * @return the PID of the spawned process or -1 on error.
 *
 * **Errors:**
 * - KECHILD The process has no child to wait on.
 */
pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1);

/**
 * Returns information about a child process that has changed
 * state. If pid is -1, this function will return when *any*
 * child has changed state, otherwise it will only return when
 * the child with PID `pid` changes state.
 *
 * By default, the only state changes tracked are RUNNING -> STOPPED
 * or * -> ZOMBIE.
 *
 * If nohang is true, then p_waitpid will return even if there are
 * no changes at the time of invocation, and will return 0 in such
 * cases. If nohang is false, then p_waitpid will block until an
 * applicable child has a change in state.
 *
 * @param[in]  pid      The PID of the child to wait for. -1 means any child.
 * @param[out] wstatus  Status flag that gives information on how a the child
                        changed state.
   @param[in]  nohang   If true, returns immediately if there are no state changes.
   @return PID of the child that changed state, 0 if there are no changes (with
           nohang = true only), or -1 on error.
 */
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);

/**
 * Sends a specified signal to a process.
 * @param[in] pid    The PID of the process to send the signal to.
 * @param[in] signal The signal to send to the process.
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - KBADSIG Signal number is not valid
 * - KBADPID Process ID does not exist
 */
int p_kill(pid_t pid, int signal);

/**
 * Immediately exits the currently running thread.
 */
void p_exit();

/**
 * Sets the priority of the given PID.
 *
 * @param[in] pid       The PID of the process to set the priority of
 * @param[in] priority  The priority to set the process to (see priority_t)
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - KBADPID The PID given is not an existing process
 * - KBADPRIO The priority given is not valid
 */
int p_nice(pid_t pid, priority_t priority);

/**
 * Sleep for the given number of ticks. Returns once enough ticks have elapsed.
 *
 * @param ticks The number of clock ticks to sleep for
 *
 * This call cannot return errors.
 */
void p_sleep(unsigned int ticks);

/**
 * Linked list node for a list of processes contained in the kernel.
 */
typedef struct ProcInfo {
    pid_t pid;             /**< The PID of the process in this node. */
    pid_t ppid;            /**< The parent PID of the process in this node (0) if no parent. */
    priority_t priority;   /**< The priority of the process pointed to by this node. */
    int state;             /**< The status of the processes pointed to by this node. */
    const char *cmd;       /**< The command-line of the process pointed to. */
    struct ProcInfo *next; /**< Pointer to the next process in the list, or NULL if tail. */
} procinfo_t;

/**
 * Returns a linked-list of process information from the kernel.
 */
procinfo_t *p_ps();

/**
 * Frees the linked-list returned by `p_ps()`.
 */
void free_procinfo(procinfo_t *proc);

#endif
