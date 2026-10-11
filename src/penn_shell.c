#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#include "shell/user_interaction.h"
#include "shell/signals.h"
#include "penn_shell.h"
#include "os/process.h"
#include "os/p_errno.h"
#include "kernel/logger.h"
#include "kernel/scheduler.h"
#include "shell/job_control.h"
#include "shell/job_list.h"
#include "os/pennfat.h"


void shell() {
    // TODO Check/parse args (not for milestone)

    umask(S_IWGRP | S_IWOTH);
    interactive = is_interactive();
    setup_signals();

    while (1) {
        JobInfo job;
        bool job_ready = try_prompt_job(&job);
        update_job_statuses(false);
        if (job_ready) {
            run_command(job);
            free_job_info(job);
        }

        handle_status_changes();// TODO This might need to move inside job
                            // execution code once we add background jobs
    }

    write_check_format(F_STDOUT_NO, "DONE!\n");
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        fprintf(stderr, "Too few arguments. Usage: %s fatfs [logfile]", argv[0]);
        exit(EXIT_FAILURE);
    }

    prompted = false;

    if (f_mount(argv[1]) < 0) {
        p_perror("Error mounting file system");
        exit(EXIT_FAILURE);
    }

    if (argc > 3) {
        fprintf(stderr, "Too many arguments. Usage: ./penn-os fatfs [logfile]");
        exit(EXIT_FAILURE);
    }

    if (argc == 3) {
        open_log_file(argv[2]);
    } else {
        open_log_file("logs");
    }

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    char *a[] = { NULL };
    k_init_scheduler(shell, a);
}
