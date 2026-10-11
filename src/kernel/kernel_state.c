#include "kernel_state.h"
extern uint64_t clock_ticks;

Node *find_in_queues(pid_t pid, LL **queue) {
    *queue = NULL;

    if (current_process != NULL &&
        current_process->pid == pid) {
        return current_process;
    }

    Node *process = locate_pid(priority_queue_high, pid);
    *queue = priority_queue_high;

    if (process == NULL) {
        process = locate_pid(priority_queue_normal, pid);
        *queue = priority_queue_normal;
    }

    if (process == NULL) {
        process = locate_pid(priority_queue_low, pid);
        *queue = priority_queue_low;
    }

    if (process == NULL) {
        process = locate_pid(stopped_queue, pid);
        *queue = stopped_queue;
    }

    if (process == NULL) {
        process = locate_pid(child_blocked_queue, pid);
        *queue = child_blocked_queue;
    }

    if (process == NULL) {
        process = locate_pid(sleep_blocked_queue, pid);
        *queue = sleep_blocked_queue;
    }

    return process;
}

void requeue_node(Node *node) {
    priority_t priority = node->process->priority;

    if (node->process->state == ZOMBIE) {
        // TODO: error handling
        return;
    }

    if (node->process->state == STOPPED) {
        insert_node_to_end(stopped_queue, node);
        return;
    }

    if (priority == P_HIGH) {
        insert_node_to_end(priority_queue_high, node);
    } else if (priority == P_NORMAL) {
        insert_node_to_end(priority_queue_normal, node);
    } else {
        insert_node_to_end(priority_queue_low, node);
    }
}

void init_kernel_state() {
    clock_ticks = 0;
    k_top_process = NULL;
    process_table = create_linked_list();
    priority_queue_low = create_linked_list();
    priority_queue_normal = create_linked_list();
    priority_queue_high = create_linked_list();
    stopped_queue = create_linked_list();
    child_blocked_queue = create_linked_list();
    sleep_blocked_queue = create_linked_list();
}
