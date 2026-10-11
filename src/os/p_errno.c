#include <stdio.h>
#include <unistd.h>
#include "p_errno.h"
#include "../kernel/kernel_state.h"

static int fallback_p_errno;

int p_errno() {
    pcb_t *process = current_process != NULL ?
        current_process->process : k_top_process;

    // When running standalone pennfat, there's no PCBs without the scheduler
    if (process != NULL) {
        return process->p_errno;
    } else {
        return fallback_p_errno;
    }
}

void p_set_errno(int p_errno) {
    pcb_t *process = current_process != NULL ?
        current_process->process : k_top_process;

    // When running standalone pennfat, there's no PCBs without the scheduler
    if (process != NULL) {
        process->p_errno = p_errno;
    } else {
        fallback_p_errno = p_errno;
    }
}

void p_perror(const char *msg) {
    if (msg != NULL) {
        dprintf(STDERR_FILENO, "%s: ", msg);
    }

    char *err_msg;
    switch (p_errno()) {
        case 0:
            err_msg = "No error";
            break;
        case PINVAL:
            err_msg = "Invalid parameter(s)";
            break;
        case PNOMEM:
            err_msg = "Failed to allocate memory";
            break;
        case POPENFAIL:
            err_msg = "Cannot open file from host";
            break;
        case PIOFAIL:
            err_msg = "Cannot read/write file from host";
            break;
        case PCLOSEFAIL:
            err_msg = "Cannot close file from host";
            break;
        case PMOUNTED:
            err_msg = "A filesystem is already mounted";
            break;
        case PCORRUPT:
            err_msg = "Filesystem is corrupt";
            break;
        case PINUSE:
            err_msg = "One or more files still open";
            break;
        case PWLOCK:
            err_msg = "File already open for writing";
            break;
        case PNOFS:
            err_msg = "A filesystem is not mounted";
            break;
        case PNOFILE:
            err_msg = "File does not exist";
            break;
        case PFULL:
            err_msg = "No more free blocks";
            break;
        case PTOOFAR:
            err_msg = "Offset falls outside file";
            break;
        case PINTERNAL:
            err_msg = "Internal error. This is a bug.";
            break;
        case KECHILD:
            err_msg = "No child process to wait on";
            break;
        case KBADFD:
            err_msg = "Invalid file descriptor";
            break;
        case KNOFD:
            err_msg = "No file descriptors left";
            break;
        case KBADPID:
            err_msg = "Invalid process ID";
            break;
        case KBADPRIO:
            err_msg = "Invalid process priority";
            break;
        case KBADSIG:
            err_msg = "Invalid signal number";
            break;
        case KBADSCHED:
            err_msg = "Invalid schedule operation.";
            break;
        default:
            err_msg = "Unknown error";
    }

    dprintf(STDERR_FILENO, "%s\n", err_msg);
}
