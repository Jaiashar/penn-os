/** @file
 * Constants and enumerations for the user-level API
 */
#ifndef TYPES_H
#define TYPES_H

#define PROCESS_RUNNING 0
#define PROCESS_BLOCKED 1
#define PROCESS_STOPPED 2
#define  PROCESS_ZOMBIE 3

#define SIGNALED(status) (status & ~(-1u >> 1)) != 0
#define MASK_SIGNAL(status) (status & (-1u >> 1))
#define W_WIFEXITED(status) (MASK_SIGNAL(status) == PROCESS_ZOMBIE)
#define W_WIFSTOPPED(status) (MASK_SIGNAL(status) == PROCESS_STOPPED)
#define W_WIFCONTINUED(status) (MASK_SIGNAL(status)== PROCESS_RUNNING)
#define W_WIFSIGNALED(status) (SIGNALED(status) && (MASK_SIGNAL(status)== PROCESS_ZOMBIE))

#define F_STDIN_NO 0
#define F_STDOUT_NO 1

#define SUCCESS 0
#define FAILURE -1

/**
 * Defines the types of signals that can be sent to running processes.
 */
enum signal_t {
    S_SIGTERM = 0, /**< Signal that when received, terminates a process. */
    S_SIGCONT = 1, /**< Signal that when received, tells a stopped process to continue
                    *   or does nothing if the target process is not stopped.
                    */
    S_SIGSTOP = 2  /**< Signal that when received, suspends a process and sets state to STOPPED. */
};

/**
 * Defines the possible priorities a process can run at.
 */
typedef enum priority_t {
    P_HIGH = -1, /**< Highest prority (-1). Processes with this priority
                  *   run 50% more frequentyly than those with normal.
                  */
    P_NORMAL = 0, /**< Normal priority (0). Processes with this priority
                   *   run 50% more frequently than those with low priority.
                   */
    P_LOW = 1     /**< Low priority (1). Processes with this priority are
                   *   scheduled the least frequently relative to processes
                   *   with other priorities.
                   */
} priority_t;
#endif
