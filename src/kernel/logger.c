

#include "logger.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "kernel_state.h"


const clockid_t clockid = CLOCK_TAI;
struct timespec start, end;

static FILE *logfile;
void get_time();

int open_log_file(char *filename) {
    //Create the log file
    logfile = fopen(filename, "w");
    if (logfile == NULL) {
        perror("Log File Creation Failed");
        return 0;
    }

    return 1;
}

void create_log_file(int pid, int nice_value, char *procname)
{
    get_time();
    fprintf(logfile, "CREATE\t\t%d\t\t%d\t\t%s\n",pid, nice_value, procname);
}

void schedule_log_file (int pid, int queue, char *procname)
{
    get_time();
    fprintf(logfile, "SCHEDULE\t%d\t\t%d\t\t%s\t\t\n",pid, queue, procname);
}

void blocked_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "BLOCKED\t\t%d\t\t%d\t\t%s\t\t\n",pid, nice, procname);
}

void waited_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "WAITED\t\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void unblocked_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "UNBLOCKED\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void signaled_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "SIGNALED\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void exited_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "EXITED\t\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void orphan_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "ORPHAN\t\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void stopped_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "STOPPED\t\t%d\t\t%d\t\t%s\n",pid, nice, procname);
}

void continue_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "CONTINUED\t\t%d\t\t%d\t\t%s\t\t\n",pid, nice, procname);
}

void zombie_log_file (int pid, int nice, char *procname)
{
    get_time();
    fprintf(logfile, "ZOMBIE\t\t%d\t\t%d\t\t%s\t\t\n",pid, nice, procname);
}

void nice_log_file (int pid, int old_nice,int new_nice, char *procname)
{

    get_time();
    fprintf(logfile, "NICE\t\t%d\t\t%d\t\t%d\t\t%s\n",pid, old_nice, new_nice, procname);
}

void flush_logs() {
    fclose(logfile);
}

void get_time(){
    fprintf(logfile,"[%jd]\t", clock_ticks);
}
