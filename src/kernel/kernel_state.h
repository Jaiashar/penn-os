/** @file
 * Defines kernel state data and operations.
 */

#ifndef KERNEL_STATE_H
#define KERNEL_STATE_H
#include "queues.h"
#include <stdint.h>

/** Number of clock ticks since the scheduler has started. */
uint64_t clock_ticks;

/** There always needs to be at least "one" running process, or else it is a system halt. */
pcb_t *k_top_process;

/** The process currently scheduled for execution. NULL means there is no scheduled process currently running. */
Node *current_process;

/** List of all non-reaped processes. */
LL *process_table;

/** Low priority queue. */
LL *priority_queue_low;

/** Normal priority queue. */
LL *priority_queue_normal;

/** High priority queue. */
LL *priority_queue_high;

/** Background queue. Scheduler will not pull from this queue. */
LL *stopped_queue;

/** Parents blocked by waitpid. */
LL *child_blocked_queue;

/** List of sleeping processes. */
LL *sleep_blocked_queue;

/**
 * Initializes all data structures needed for kernel (scheduler) operation.
 */
void init_kernel_state();

/**
 * Searches through all queues for the PCB node with the specified
 * process ID, and outputs the queue it was found in.
 *
 * TODO: This is essentially a linear PCB lookup. We may want to
 *       consider optimizing this.
 *
 * @param[in]  pid  The PID of the process to find.
 * @param[out] queue  The Linked List queue the PCB was found in.
 * @return the PCB node if found. NULL if no PCB with the given PID
 *         could be found.
 */
Node *find_in_queues(pid_t pid, LL **queue);

/**
 * Queues a given PCB node back into the appropriate
 * process queue, based on priority and state.
 *
 * @param[in] node  The node (with PCB pointer) to enqueue.
 */
void requeue_node(Node *node);
#endif
