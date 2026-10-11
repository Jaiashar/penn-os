#include"kernel/scheduler.h"
#include"os/process.h"
#include <signal.h>
#include <stdint.h>

static pid_t pid1;
void counter1()
{
    static uint64_t count = 0;
    while (1)
    {
        if ((count++ % 10000000) == 0)
            dprintf(STDERR_FILENO, "Counter1: %lu\n", count);

        if (count == 160000000) {
            dprintf(STDERR_FILENO, "Termming....\n");
            p_kill(pid1, S_SIGSTOP);
        }
        if (count >= 180000000) {
            break;
        }
    }
}

void counter2()
{
    static uint64_t count2 = 0;
    while (1)
    {
        if ((count2++ % 10000000) == 0) {
            dprintf(STDERR_FILENO, "Counter2: %lu\n", count2);
        }

        if (count2 == 320000000) {
            dprintf(STDERR_FILENO, "Continuing....\n");
            p_kill(pid1, S_SIGCONT);
            break;
        }
    }
}

void start()
{
    pid1 = p_spawn(counter1, NULL);
    p_nice(pid1, P_LOW);
    p_spawn(counter2, NULL);

    int status;
    dprintf(STDERR_FILENO, "Waiting on child...\n");
    p_waitpid(-1, &status, false);
    dprintf(STDERR_FILENO, "Child status changed...\n");
}

int main()
{
    //signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    k_init_scheduler(start, NULL);
}
