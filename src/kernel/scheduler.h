/** @file
 * Kernel API for scheduler-related operations.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <ucontext.h>
#include <stdbool.h>
#include <unistd.h>
#include "queues.h"
#include "process_state.h"

extern sig_atomic_t can_preempt;

/**
 * Initializes a new pcb_t struct.
 * @param[in] parent Pointer to the PCB of the spawning process.
 *                   Can be NULL if there is none.
 * @param[in] fd0    File descriptor to be duplicated to FD 0 (STDIN)
 * @param[in] fd1    File descriptor to be duplicated to FD 1 (STDOUT)
 * @return Pointer to the PCB of the newly created process.
 */
pcb_t *k_process_create(pcb_t *parent, int fd0, int fd1);

/**
 * Sends a specified signal to a process.
 * @param[in] pid    The PID of the process to send the signal to.
 * @param[in] signal The signal to send to the process.
 * @return -1 on failure, 0 on success.
 *
 * Errors:
 * - KBADPID pid parameter does not exist in process table.
 * - KBADSIG signal parameter is not a valid signal.
 */
int k_process_kill(pid_t pid, int signal);

/**
 * Cleans up the specified PCB by:
 *     - Removing it from the process table.
 *     - Removing its presence in any of the canonical scheduler queues
 *       (p_high, p_normal, p_low, blocked, background)
 *     - Recursively cleaning any children.
 *     - Clearing the execution context and PCB structure.
 *     - Clearing open file descriptors.
 *
 * @param[in] pcb  Pointer to PCB structure to clean up.
 * @return 0 on success or -1 on failure.
 *
 * Errors:
 * - KBADPID process or one of it's children not present in the process table.
 */
int k_process_cleanup(pcb_t *process);

/**
 * Initializes the scheduler and starts a provided "root" process.
 * @param[in] fun  Pointer to the function of the root process.
 * @param[in] argv Arguments to run the "root" process with.
 */
void k_init_scheduler(void (*func)(), char *argv[]);

/**
 * Schedules a process for execution by putting it in it's appropriate priority queue,
 * round-robin style.
 * @param[in] process The initialized PCB of the process to schedule.
 * @return 0 if successful, -1 on failure.
 *
 * Errors:
 * - KBADSCHED process PCB not in valid state.
 */
int k_schedule_process(pcb_t *process);

/**
 * Initializes a ucontext_t struct for use with the scheduler.
 * @param[in] ucp  Struct to initialize.
 * @param[in] func Pointer to function to initialize context at.
 * @param[in] argv Argument array ucontext should invoke with function.
 */
void k_makecontext(ucontext_t *ucp, void (*func)(), char*argv[]);

/**
 * Sets the priority of a process and moves it to the appropriate
 * scheduler queue.
 * @param[in] pid      The PID of the process to set priority of.
 * @param[in] priority The priority to set the process to.
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - KBADPID The PID given is not an existing process
 * - KBADPRIO The priority given is not valid
 */
int k_set_priority(pid_t pid, int priority);

/**
 * Immediately jumps execution to the top of the scheduler loop.
 */
void k_trap_to_scheduler();

/**
 * Sets the currently running process to "ZOMBIE" status and traps
 * to the scheduler.
 */
void k_zombify_current_process();

/**
 * @return The PID of the currently scheduled process.
 */
pid_t k_curr_pid();

/**
 * Sets the scheduler in kernel mode where it will not
 * pre-empt running code. Essentially disables scheduler
 * interrupts.
 */
void k_set_kernel_handler();

/**
 * Sets the scheduler in user mode where it will pre-empt
 * running code to invoke the scheduler. (Re-)enables
 * scheduler interrupts.
 */
void k_set_user_handler();

/**
 * Allocates a file descriptor for the currently scheduled
 * process to the file.
 * @param[in] file  The file handle to allocate an FD to.
 * @return The allocated FD or -1 on failure.
 *
 * Errors:
 * - KNOFD there are no file descriptors left in the process table.
 */
int k_set_fd(filehandle_t *file);

/**
 * Gets the file handle reference associated with the given
 * file descriptor.
 * @param[in] fd The file descriptor to fetch the handle for.
 * @return the file handle reference type
 *
 * Errors:
 * - KBADFD fd is out of range or does not point to a file.
 */
fhref_t *k_get_fd(int fd);

/**
 * Gets the file handle reference associated with the given
 * file descriptor, and clears it if one is found.
 * @param[in] fd The file descriptor to fetch the handle for.
 * @return the file handle reference type
 *
 * Errors:
 * - KBADFD fd is out of range or does not point to a file.
 */
fhref_t *k_clear_fd(int fd);

#endif
