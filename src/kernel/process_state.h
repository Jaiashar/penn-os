#ifndef PROCESS_STATE_H
#define PROCESS_STATE_H
#include <ucontext.h>
#include <sys/types.h>
#include <stdint.h>
#include "../os/types.h"
#include "../pennfat/file_handle.h"
#include "queues.h"

#define QUANTUM 100000
#define CONTEXT_STACK_SIZE 16384
#define OPEN_FILE_COUNT 32

// Forward linked-list declaration due to circular
// dependency.
typedef struct LL LL;

/**
 * Defines the different stages of a process lifecycle.
 */
typedef enum state_t {
    RUNNING = PROCESS_RUNNING, /**< Process is running and can be scheduled at any time. */
    BLOCKED = PROCESS_BLOCKED, /**< Process is blocked by some resource and will not be scheduled. */
    STOPPED = PROCESS_STOPPED, /**< Process is suspended until signaled to continue. */
    ZOMBIE =  PROCESS_ZOMBIE   /**< Process has terminated and is waiting to be reaped. */
} state_t;

/**
 * Enumerates the special (non-FS) file references that can
 * appear in the file table.
 */
typedef enum sft_t {
    NONE,   /**< No special type. The file reference refers to an actual file. */
    STDOUT, /**< The file reference points to STDOUT. */
    STDIN   /**< The file reference points to STDIN. */
} sft_t;

typedef struct fhref_t {
    sft_t special_type;
    filehandle_t *handle;
    bool used;
} fhref_t;

// Note: While linked-list Nodes may be duplicated for different lists,
// there should only be one "source" pcb allocation in memory at a time
// for each process.
typedef struct pcb {
    pid_t pid;                   /**< Process ID (PID) of the process. */
    struct pcb *parent;          /**< Pointer to the PCB of the parent/spawning process. */
    ucontext_t *exec_ctx;        /**< Stack + machine registers (incl. stack pointer, program counter, etc). */
    priority_t priority;         /**< Priority of the process: -1, 0, 1. Lower is higher. */
    state_t state;               /**< State of the process. Can be one of: RUNNING, BLOCKED, STOPPED, ZOMBIE */
    LL *zombie_queue;            /**< Linked-list of all chidren that are in ZOMBIE status. */
    LL *child_wait_queue;        /**< Linked-list of all children with state changes. */
    LL *children;                /**< Linked-list of all children. List of all children. */
    fhref_t open_files[OPEN_FILE_COUNT]; /**< Array of file descriptors in use by the proces. */
    uint64_t start_clock_tick;   /**< Kernel clock tick this process was started at. */
    uint64_t sleep_until_tick;   /**< If the process is sleeping, sleep until this many ticks. */
    int p_errno;                 /**< Last error code for this process. Undefined if there never was an error. */
    bool signaled;               /**< True if the process was terminated by a signal. */
    char procname[32];           /**< Command string used to execute the process ala `argv[0]`. */
} pcb_t;
#endif
