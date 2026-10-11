#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "kernel_state.h"
#include "logger.h"
#include <valgrind/valgrind.h>
#include "../os/types.h"
#include "../os/p_errno.h"
#include "../pennfat/file_handle.h"

volatile int next_pid = 1;
static ucontext_t *schedulerContext;
static ucontext_t *idle_ctx;
static void set_process_state(Node *node, LL *queue, state_t state);

sig_atomic_t can_preempt = 1;

static void setStack(stack_t *stack) {
    // allocate stack for each context - from sched_demo.c
    void *sp = malloc(CONTEXT_STACK_SIZE);

    // Let valgrind know that the memory area allocated is
    // effectively a stack. Helps for debugging.
    VALGRIND_STACK_REGISTER(sp, sp + CONTEXT_STACK_SIZE);
    *stack = (stack_t){.ss_sp = sp, .ss_size = CONTEXT_STACK_SIZE};
}

static void setTimer(void) {
    struct itimerval it;
    it.it_interval = (struct timeval){ .tv_usec = QUANTUM };
    it.it_value = it.it_interval;
    setitimer(ITIMER_REAL, &it, NULL);
}

// on SIGALRM, go to the scheduler
static void alarmHandler(int signum) {
    if (!can_preempt) {
        can_preempt = 1;
        return;
    }

    clock_ticks++;
    ucontext_t *context = current_process == NULL ?
        idle_ctx : current_process->process->exec_ctx;
    swapcontext(context, schedulerContext);
}

Node *get_next_process_to_execute() {
    static int priority_position = 0;
    static int priority_position2 = 0;
    static int priority_position3 = 0;
    static int priority_rotation[19] = { -1, 0, -1, 1, -1, 0, -1, 1, 0, -1, 0, -1, 0, -1, 1, -1, 0, -1, 1 };
    static int priority_rotation2[5] = { 0, 1, 0, 1, 0 };
    static int priority_rotation3[13] = { -1, -1, -1, 1, -1, -1, 1, -1, -1, 1, -1, -1, 1 };

    if (priority_queue_high->head != NULL &&
        priority_queue_normal->head != NULL &&
        priority_queue_low->head != NULL) {
        int priority = priority_rotation[priority_position];

        if (++priority_position > 18) {
            priority_position = 0;
        }

        if (priority == -1) {
            return dequeue_front(priority_queue_high);
        } else if (priority == 0) {
            return dequeue_front(priority_queue_normal);
        } else {
            return dequeue_front(priority_queue_low);
        }
    } else if (priority_queue_high->head != NULL &&
               priority_queue_normal->head != NULL) {
        int priority = priority_rotation2[priority_position2];

        if (++priority_position2 > 4) {
            priority_position2 = 0;
        }

        if (priority == 0) {
            return dequeue_front(priority_queue_high);
        } else {
            return dequeue_front(priority_queue_normal);
        }
    } else if (priority_queue_normal->head != NULL &&
              priority_queue_low->head != NULL) {
        int priority = priority_rotation2[priority_position2];

        if (++priority_position2 > 4) {
            priority_position2 = 0;
        }

        if (priority == 0) {
            return dequeue_front(priority_queue_normal);
        } else {
            return dequeue_front(priority_queue_low);
        }
    } else if (priority_queue_high->head != NULL &&
               priority_queue_low->head != NULL) {
        int priority = priority_rotation3[priority_position3];

        if (++priority_position3 > 12) {
            priority_position3 = 0;
        }

        if (priority == -1) {
            return dequeue_front(priority_queue_high);
        } else {
            return dequeue_front(priority_queue_low);
        }
    } else if (priority_queue_normal->head != NULL) {
        return dequeue_front(priority_queue_normal);
    } else if (priority_queue_high->head != NULL) {
        return dequeue_front(priority_queue_high);
    } else {
        return dequeue_front(priority_queue_low);
    }
}

// Forward declaration since process_children_cleanup and
// process_cleanup are mutually-recursive.
static void process_children_cleanup();

// Destroy process by node and reap all children.
static void process_children_cleanup(pcb_t *pcb) {
    // Go ahead and clear the PCBs for *all* children.
    // This will effectively force-kill them.
    if(pcb->children == NULL) {
        return;
    }
    for (Node *n = pcb->children->head; n != NULL; n = n->next) {
        orphan_log_file(n->process->pid, n->process->priority,n->process->procname);
        k_process_cleanup(n->process);
    }

    // Now that all the child PCBs are gone, each list can be cleared
    // in a shallow manner.
    dispose_list(pcb->child_wait_queue);
    pcb->child_wait_queue = NULL;
    dispose_list(pcb->zombie_queue);
    pcb->zombie_queue = NULL;
    dispose_list(pcb->children);
    pcb->children = NULL;
}

void incrementSystemClock() {
    clock_ticks++;
}

void update_sleeping_processes() {
    Node *proc = sleep_blocked_queue->head;
    while(proc != NULL) {
        Node *next = proc->next;
        if (clock_ticks >= proc->process->sleep_until_tick) {
            unblocked_log_file(proc->process->pid, proc->process->priority, proc->process->procname);
            remove_from_list(sleep_blocked_queue, proc);
            proc->process->state = RUNNING;
            requeue_node(proc);
        }
        proc = next;
    }
}

static void system_halt() {
    free(idle_ctx->uc_link->uc_stack.ss_sp);
    free(idle_ctx->uc_link);
    free(idle_ctx->uc_stack.ss_sp);
    free(idle_ctx);
    free(schedulerContext->uc_stack.ss_sp);
    free(schedulerContext);
    free(current_process);
    dispose_list(priority_queue_high);
    dispose_list(priority_queue_low);
    dispose_list(priority_queue_normal);
    dispose_list(sleep_blocked_queue);
    dispose_list(child_blocked_queue);
    dispose_list(stopped_queue);
    dispose_list(process_table);
    flush_logs();
    exit(EXIT_SUCCESS);
}

static void scheduler() {
    // ignore sigalrm when context switching - we should never stop the system clock
    k_set_kernel_handler();
    update_sleeping_processes();
    Node *n = current_process;
    if (current_process == NULL) {
        // initially no process is scheduled, so directly retrieve from the queue
        current_process = get_next_process_to_execute();
    } else {
        // free process if it completed execution
        if (current_process->process->state == ZOMBIE) {
            // Process has no parent, so fully reap the process.
            // TODO: Since the only process with no parent is the "top" process,
            //       it may be more accurate to make this a system halt.
            if (current_process->process->parent == NULL) {
                k_process_cleanup(current_process->process);
                system_halt();
            }
        } else if (current_process->process->state != BLOCKED) {
            // if process was interrupted, add it to the queue again
            requeue_node(current_process);
        }

        current_process = get_next_process_to_execute();
    }

    // unblock sigalrm interrupts
    k_set_user_handler();
    if (current_process == NULL) {
        setcontext(idle_ctx);
    } else {
        if (n == NULL || current_process->pid != n->pid)
            schedule_log_file(current_process->process->pid, current_process->process->priority, current_process->process->procname);
        setcontext(current_process->process->exec_ctx);
    }
}

static void idle_process() {

    while (1) {
        sigset_t mask;
        sigemptyset(&mask);
        sigsuspend(&mask);
    }
}

static ucontext_t *get_idle_ctx() {
    ucontext_t *ctx = malloc(sizeof(ucontext_t));
    k_makecontext(ctx, idle_process, 0);
    return ctx;
}

/**
 * Initializes the "top" process. The top process is the parent
 * of all other processes and is the only process without a parent
 * itself. For the purposes of this PennOS implementation, this
 * process will usually be the shell loop.
 *
 * TODO: There is some logic shared here with k_process_create
 *       as defined below. We may want to factor this out into
 *       a common method.
 *
 * @param[in] func  Pointer to function defining the behavior of the top
 *                  process.
 * @param[in] argv  argument array for the provided function.
 * @return PCB of created top process.
 */
static pcb_t *init_top_process(void (*func)(), char *argv[]) {
    pcb_t *top_process = malloc(sizeof(pcb_t));
    top_process->parent = NULL;
    top_process->priority = P_NORMAL;
    top_process->pid = next_pid++;
    top_process->state = RUNNING;
    top_process->children = create_linked_list();
    top_process->zombie_queue = create_linked_list();
    top_process->child_wait_queue = create_linked_list();
    top_process->start_clock_tick = clock_ticks;
    top_process->sleep_until_tick = 0;
    top_process->signaled = false;
    strcpy(top_process->procname, "shell");

    top_process->open_files[F_STDOUT_NO].special_type = STDOUT;
    top_process->open_files[F_STDOUT_NO].used = true;
    top_process->open_files[F_STDIN_NO].special_type = STDIN;
    top_process->open_files[F_STDIN_NO].used = true;

    for (int i = 2; i < OPEN_FILE_COUNT; i++) {
        top_process->open_files[i].special_type = NONE;
        top_process->open_files[i].handle = NULL;
        top_process->open_files[i].used = false;
    }

    create_log_file(top_process->pid, top_process->priority, top_process->procname);
    nice_log_file(top_process->pid, top_process->priority, P_HIGH, top_process->procname);
    top_process->priority = P_HIGH;

    ucontext_t *ctx = malloc(sizeof(ucontext_t));
    k_makecontext(ctx, func, argv);
    top_process->exec_ctx = ctx;

    insert_to_end(process_table, top_process);
    k_top_process = top_process;
    return top_process;
}

static void set_process_state(Node *node, LL *queue, state_t state) {
    pcb_t *pcb = node->process;

    if (pcb->state == state) {
        return;
    }

    k_set_kernel_handler();
    pcb->state = state;

    // NOTE: Something to consider:
    //       What happens if the process is blocked, and we are sending
    //       S_SIGSTOP? If it later continues, that means the process
    //       will go to RUNNING rather than BLOCKED. If the process is
    //       waiting on a child, it might be okay since p_waitpid will
    //       immediately block it again. However, this will waste CPU time.
    if (queue != NULL) {
        remove_from_list(queue, node);
        requeue_node(node);
    }

    // If the process has a parent, state changes should
    // put the child in the wait queue.
    if (pcb->parent != NULL && pcb->state != RUNNING) {
        Node *found;
        if ((found = locate_pid(pcb->parent->child_wait_queue, pcb->pid)) != NULL) {
            remove_from_list(pcb->parent->child_wait_queue, found);
        }

        insert_to_end(pcb->parent->child_wait_queue, pcb);
    }

    if (pcb->state == ZOMBIE) {
        process_children_cleanup(pcb);
    }

    k_set_user_handler();

    // For each process that is blocked by waiting for children,
    // check to see if it is a parent of this process. If it is,
    // unblock the parent since it may unblock p_waitpid.
    for (Node *n = child_blocked_queue->head; n != NULL; n = n->next) {
        if (pcb->parent != NULL &&
            n->process->pid == pcb->parent->pid) {
            unblocked_log_file(n->process->pid, n->process->priority, n->process->procname);
            set_process_state(n, child_blocked_queue, RUNNING);
        }
    }

}

void k_zombify_current_process() {
    // this method is set as the default successor to
    // all methods. When a method completes execution,
    // it enters a new context with this method through uc_link
    // and the process state is set to ZOMBIE here
    // https://piazza.com/class/kjfxgchngcs2h1?cid=1146
    LL *queue;
    find_in_queues(current_process->pid, &queue);
    zombie_log_file(current_process->process->pid, current_process->process->priority, current_process->process->procname);
    set_process_state(current_process, queue, ZOMBIE);
    swapcontext(current_process->process->exec_ctx, schedulerContext);
}

void k_trap_to_scheduler() {
    ucontext_t *context = current_process == NULL ?
        idle_ctx : current_process->process->exec_ctx;

    swapcontext(context, schedulerContext);
}

int k_schedule_process(pcb_t *process) {
    if (process->state != RUNNING) {
        p_set_errno(KBADSCHED);
        return FAILURE;
    }

    if (process->priority == P_HIGH) {
        insert_to_end(priority_queue_high, process);
    } else if (process->priority == P_NORMAL) {
        insert_to_end(priority_queue_normal, process);
    } else {
        insert_to_end(priority_queue_low, process);
    }

    return SUCCESS;
}

pcb_t *k_process_create(pcb_t *parent, int fd0, int fd1) {
    // create a new process with appropriate parent ID
    // with normal priority, and starting out as READY
    // to run (in PennOS, Running and ready are equivalent)
    pcb_t *new_process = malloc(sizeof(pcb_t));
    new_process->parent = parent;

    // Writeup says child processes should have a default
    // priority of normal.
    new_process->pid = next_pid++;
    new_process->state = RUNNING;

    new_process->zombie_queue = create_linked_list();
    new_process->child_wait_queue = create_linked_list();
    new_process->children = create_linked_list();
    new_process->start_clock_tick = clock_ticks;
    new_process->sleep_until_tick = 0;
    new_process->signaled = false;

    for (int i = 0; i < OPEN_FILE_COUNT; i++) {
        new_process->open_files[i] = parent->open_files[i];
        if (new_process->open_files[i].special_type == NONE &&
            new_process->open_files[i].used) {
            fh_incref(new_process->open_files[i].handle);
        }
    }

    if (fd0 != F_STDIN_NO || fd1 != F_STDOUT_NO) {
        // dup2 functionality
        //TODO Need to disable context switching here
        if (fd0 != 0) {
            if (new_process->open_files[0].special_type == NONE) {
                fh_close(new_process->open_files[0].handle);
            }

            new_process->open_files[0] = new_process->open_files[fd0];
            if (new_process->open_files[fd0].special_type == NONE) {
                new_process->open_files[fd0].used = false;
                new_process->open_files[fd0].handle = NULL;
            }
        }
        if (fd1 != 1) {
            if (new_process->open_files[1].special_type == NONE) {
                fh_close(new_process->open_files[1].handle);
            }
            new_process->open_files[1] = new_process->open_files[fd1];
            if (new_process->open_files[fd1].special_type == NONE) {
                new_process->open_files[fd1].used = false;
                new_process->open_files[fd1].handle = NULL;
            }
        }
    }

    insert_to_end(parent->children, new_process);
    new_process->priority = P_NORMAL;

    insert_to_end(process_table, new_process);
    return new_process;
}

void k_init_scheduler(void (*func)(), char *argv[]) {
    schedulerContext = malloc(sizeof(ucontext_t));
    k_set_user_handler();
    setTimer();

    init_kernel_state();
    idle_ctx = get_idle_ctx();
    k_schedule_process(init_top_process(func, argv));
    k_makecontext(schedulerContext, scheduler, NULL);
    setcontext(schedulerContext);
}

int k_process_kill(pid_t pid, int signal) {
    LL *queue;
    Node *node = find_in_queues(pid, &queue);

    if (node == NULL) {
        p_set_errno(KBADPID);
        return FAILURE;
    }

    // If target is a zombie process, it can't receive signals.
    if (node->process->state == ZOMBIE) {
        return SUCCESS;
    }

    switch (signal) {
    case S_SIGTERM:
        node->process->signaled = true;

        signaled_log_file(node->process->pid, node->process->priority, node->process->procname);
        zombie_log_file (node->process->pid, node->process->priority, node->process->procname);
        set_process_state(node, queue, ZOMBIE);
        break;
    case S_SIGCONT:
        if (node->process->state != STOPPED) {
            return SUCCESS;
        }

        continue_log_file(node->process->pid, node->process->priority, node->process->procname);
        set_process_state(node, queue, RUNNING);
        break;
    case S_SIGSTOP:
        if (node->process->state == STOPPED) {
            return SUCCESS;
        }

        stopped_log_file(node->process->pid, node->process->priority, node->process->procname);
        set_process_state(node, queue, STOPPED);
        break;
    default:
        p_set_errno(KBADSIG);
        return FAILURE;
    }

    return SUCCESS;
}

int k_set_priority(pid_t pid, int priority) {
    if (current_process != NULL &&
        current_process->pid == pid) {
        current_process->process->priority = priority;
        return SUCCESS;
    }

    LL *queue;
    Node *process = find_in_queues(pid, &queue);
    if (process == NULL) {
        p_set_errno(KBADPID);
        return FAILURE;
    }

    if (priority == process->process->priority) {
        return SUCCESS;
    }

    remove_from_list(queue, process);

    k_set_kernel_handler();
    process->process->priority = priority;
    requeue_node(process);
    k_set_user_handler();

    return SUCCESS;
}

int k_process_cleanup(pcb_t *pcb) {
    // TODO Need to disable context switching here
    for (int i = 0; i < OPEN_FILE_COUNT; i++) {
        if (pcb->open_files[i].special_type == NONE &&
            pcb->open_files[i].used) {
            fh_close(pcb->open_files[i].handle);
            pcb->open_files[i].used = false;
        }
    }

    LL *q;
    Node *node = find_in_queues(pcb->pid, &q);
    if (node != NULL && q != NULL) {
        remove_from_list(q, node);
    }

    node = locate_pid(process_table, pcb->pid);
    if (node != NULL) {
        remove_from_list(process_table, node);
        free(node);
    } else {
        p_set_errno(KBADPID);
        return FAILURE;
    }

    process_children_cleanup(pcb);

    free(pcb->exec_ctx->uc_stack.ss_sp);
    if (pcb->exec_ctx->uc_link != NULL) {
        free(pcb->exec_ctx->uc_link->uc_stack.ss_sp);
        free(pcb->exec_ctx->uc_link);
    }

    free(pcb->exec_ctx);
    free(pcb);

    return SUCCESS;
}

/**
 * Create a context to execute a function with corresponding args.
 */
void k_makecontext(ucontext_t *ucp, void (*func)(), char *argv[]) {
    // TODO: add signal handling
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);
    setStack(&ucp->uc_stack);

    if (func == scheduler) {
        ucp->uc_link = NULL;
        makecontext(ucp, func, 0);
    } else {
        // set the uc_link for all non-scheduler processes
        // as the "zombify" method
        ucontext_t *uc = malloc(sizeof(ucontext_t));
        getcontext(uc);
        setStack(&uc->uc_stack);
        makecontext(uc, k_zombify_current_process, 0);
        ucp->uc_link = uc;

        // create the context for the spawned method
        makecontext(ucp, func, 1, argv);
    }
}

pid_t k_curr_pid() {
    return current_process->pid;
}

void k_set_kernel_handler() {
    struct sigaction act;
    act.sa_handler = incrementSystemClock;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

void k_set_user_handler() {
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

int k_set_fd(filehandle_t *file) {
    pcb_t *process = current_process != NULL ?
        current_process->process : k_top_process;

    for (int i = 0; i < OPEN_FILE_COUNT; i++) {
        fhref_t *fh = &process->open_files[i];
        if (!fh->used) {
            fh->handle = file;
            fh->used = true;
            fh->special_type = NONE;

            return i;
        }
    }

    // If we've reached this point, the process FD table
    // is full, and we must error.
    p_set_errno(KNOFD);
    return -1;
}

fhref_t *k_get_fd(int fd) {
    pcb_t *process = current_process != NULL ?
        current_process->process : k_top_process;

    fhref_t *fh;
    if (fd < 0 ||
        fd >= OPEN_FILE_COUNT ||
        !(fh = &process->open_files[fd])->used) {
        p_set_errno(KBADFD);
        return NULL;
    }

    return fh;
}

fhref_t *k_clear_fd(int fd) {
    pcb_t *process = current_process != NULL ?
        current_process->process : k_top_process;

    fhref_t *fh;
    if (fd < 0 ||
        fd >= OPEN_FILE_COUNT ||
        !(fh = &process->open_files[fd])->used ||
        fh->special_type != NONE) {
        p_set_errno(KBADFD);
        return NULL;
    }

    fh->used = false;
    return fh;
}
