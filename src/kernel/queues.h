#ifndef QUEUES_H
#define QUEUES_H
#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include "process_state.h"

typedef struct pcb pcb_t;

/**
 * Linked list node representing a process in a process queue.
 */
typedef struct Node
{
    pcb_t *process;     /**< A pointer to the PCB of the process in the list. */
    pid_t pid;          /**< PID of the PCB pointed to by this node. */
    struct Node *next;  /**< The next node/PCB in the linked list. NULL if none. */
    struct Node *prev;  /**< The previous node/PCB in the linkes list. NULL if none. */
} Node;


/**
 * Simple linked list for storing PCB information.
 */
typedef struct LL
{
    int size;   // Total number of PCBs in the list.
    Node *head; // First PCB in the list.
    Node *tail; // Last PCB in the list.
} LL;
LL *create_linked_list();

/**
 * Inserts a PCB pointer to the end (tail) of a list.
 * Allocates a new node for the given PCB.
 * @param list    The linked list to add to the end of.
 * @param process The process/PCB to add to the list.
 */
void insert_to_end(LL *list, pcb_t *process);

/**
 * Inserts an orphaned linked list node pointer to the end
 * (tail) of a linked list.
 * @param list  The linked list to add to the end of.
 */
void insert_node_to_end(LL *list, Node *node);

/**
 * Removes and returns a linked list node from the
 * head of the list.
 * @param list  The list to remove the head from.
 * @return a pointer to the removed node or NULL if the list
 *         is empty.
 */
Node *dequeue_front(LL *list) ;

/**
 * Searches a linked list for a node with a given PID.
 * @param list  the list to search.
 * @param pid   the PID to search for.
 * @return a pointer to the found Node or NULL if none found.
 */
Node *locate_pid(LL *list, int pid);

/**
 * Removes a node from the list.
 * @param list  the List to remove from.
 * @param node  the node to remove.
 */
void remove_from_list(LL *list, Node *node);

/**
 * Cleans up the provided list object and all its nodes.
 * NOTE: Does not clean up the PCBs the list points to.
 * @param list The list to dispose of.
 */
void dispose_list(LL *list);
#endif
