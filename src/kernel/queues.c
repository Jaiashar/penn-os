#include <sys/types.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include "queues.h"

LL *create_linked_list()
{
    LL *list = malloc(sizeof(LL));
    list->head = list->tail = NULL;
    list->size = 0;
    return list;
}

void insert_node_to_end(LL *list, Node *node) {
    list->size++;
    if (!list->head) {
        list->head = list->tail = node;
    }
    else {
        list->tail->next = node;
        node->prev = list->tail;
        list->tail = node;
    }
}

void insert_to_end(LL *list, pcb_t *process)
{
    Node *node = malloc(sizeof(Node));
    node->pid = process->pid;
    node->process = process;
    node->next = NULL;
    node->prev = NULL;

    insert_node_to_end(list, node);
}

Node *dequeue_front(LL*list) {
    if (!list->head) {
        return NULL;
    }

    list->size--;
    if (list->head == list->tail) {
        Node *node = list->head;
        list->head = list->tail = NULL;
        return node;
    }

    Node *node = list->head;
    list->head = list->head->next;
    list->head->prev = NULL;

    node->next = NULL;
    return node;
}

Node *locate_pid(LL *list, int pid) {
    Node *node = list->head;
    if (!node) {
        return NULL;
    }

    while (node != NULL) {
        if (node->pid == pid) {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

void remove_from_list(LL *list, Node *node) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    if (list->head == node) {
        list->head = node->next;
    }
    node->next = NULL;

    if (list->tail == node) {
        list->tail = node->prev;
    }
    node->prev = NULL;
}

void dispose_list(LL *list) {
    Node *n = list->head;
    while (n != NULL) {
        Node *old = n;
        n = n->next;
        free(old);
    }

    free(list);
}
