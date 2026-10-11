#include "process.h"
#include "../kernel/process_state.h"
#include "../kernel/scheduler.h"
#include "../kernel/kernel_state.h"
#include "../kernel/logger.h"
#include "p_errno.h"
#include <stdbool.h>
#include <string.h>

pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1) {
    pcb_t *parent = current_process->process;
    pcb_t *process = k_process_create(parent, fd0, fd1);
    strcpy(process->procname, argv[0]);
    create_log_file(process->pid, process->priority, process->procname);

    // create a new context (p_spawn is the equivalent of calling fork and then exec)
    ucontext_t *ctx = malloc(sizeof(ucontext_t));
    k_makecontext(ctx, func, argv);
    process->exec_ctx = ctx;

    // schedule the created process to run
    if (k_schedule_process(process) == FAILURE) {
        return FAILURE;
    }

    return process->pid;
}

int p_kill(pid_t pid, int signal) {
    return k_process_kill(pid, signal);
}

int p_nice(pid_t pid, int priority) {
    pcb_t *proc = current_process->process;
    nice_log_file(proc->pid, proc->priority, priority, proc->procname);

    if (k_set_priority(pid, priority) == FAILURE) {
        return FAILURE;
    }

    return 0;
}

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang) {
    if (current_process->process->children == NULL || current_process->process->children->head == NULL) {
        p_set_errno(KECHILD);
        return -1;
    }

    // TODO: *NASTY* race condition potential here. Assume there is nothing in
    //       the child_wait_queue here, and the PC goes to the line that sets
    //       the current process's status to BLOCKED. Then we context switch to
    //       a process that is changing a child's state. Then, set_process_state
    //       will update child_wait_queue, and set this process to running. However,
    //       since we are already outside of the do-while loop, we will not get
    //       the updated child status, and will block until another child state change
    //       occurs.
    do {
        for (Node *n = current_process->process->child_wait_queue->head; n != NULL; n = n->next) {
            pcb_t *proc = n->process;
            if (proc->parent != NULL && (proc->pid == pid || pid == -1)) {
                remove_from_list(current_process->process->child_wait_queue, n);

                pid_t found = proc->pid;
                if (wstatus != NULL) {
                    *wstatus = proc->state;
                    if (n->process->signaled) {
                        // set MSB to 1 if signaled
                        *wstatus |= ~(-1u >> 1);
                    }
                }

                // As per @1253 on Piazza, we only return when
                // children exit or are stopped.
                if (proc->state != STOPPED && proc->state != ZOMBIE) {
                    continue;
                }

                waited_log_file(found, n->process->priority, n->process->procname);
                if (n->process->state == ZOMBIE) {
                    Node *zombie = locate_pid(current_process->process->children, n->process->pid);
                    remove_from_list(current_process->process->children, zombie);
                    k_process_cleanup(n->process);
                    free(zombie);
                }

                free(n);
                return found;
            }
        }

        if (nohang) {
            return 0;
        }

        // If no child is found, block self and context switch to scheduler.
        blocked_log_file(current_process->process->pid, current_process->process->priority, current_process->process->procname);
        current_process->process->state = BLOCKED;
        insert_node_to_end(child_blocked_queue, current_process);
        can_preempt = 0;
        k_trap_to_scheduler();
    } while (1);

    return 0;
}

void p_exit() {
    exited_log_file(current_process->pid, current_process->process->priority, current_process->process->procname);
    k_zombify_current_process();
    return;
}

void p_sleep(unsigned int ticks) {
    current_process->process->sleep_until_tick = clock_ticks + ticks;
    while (clock_ticks < current_process->process->sleep_until_tick) {
        blocked_log_file(current_process->process->pid, current_process->process->priority, "sleep");
        insert_node_to_end(sleep_blocked_queue, current_process);
        current_process->process->state = BLOCKED;
        can_preempt = 0;
        k_trap_to_scheduler();
    }

    return;
}

procinfo_t *p_ps() {
    procinfo_t *prev = NULL;
    procinfo_t *first = NULL;
    for (Node *node = process_table->head; node != NULL; node = node->next) {
        procinfo_t *proc = malloc(sizeof(procinfo_t));
        proc->pid = node->process->pid;
        proc->ppid = node->process->parent == NULL ? 0 : node->process->parent->pid;
        proc->priority = node->process->priority;
        proc->state = node->process->state;
        proc->cmd = node->process->procname;
        proc->next = NULL;

        if (prev != NULL) {
            prev->next = proc;
        } else if (first == NULL) {
            first = proc;
        }

        prev = proc;
    }

    return first;
}

void free_procinfo(procinfo_t *proc) {
    procinfo_t *p = proc;
    while (p != NULL) {
        procinfo_t *old = p;
        p = p->next;
        free(old);
    }
}

