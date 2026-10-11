#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

#define MAX_FILE_PATH_LEN 256

void create_log_file(int pid, int nice_value, char *procname);
void schedule_log_file (int pid, int queue, char *procname);
void blocked_log_file (int pid, int nice, char *procname);
void waited_log_file (int pid, int nice, char *procname);
void unblocked_log_file (int pid, int nice, char *procname);
void signaled_log_file (int pid, int nice, char *procname);
void exited_log_file (int pid, int nice, char *procname);
void orphan_log_file (int pid, int nice, char *procname);
void stopped_log_file (int pid, int nice, char *procname);
void continue_log_file (int pid, int nice, char *procname);
void zombie_log_file (int pid, int nice, char *procname);
void nice_log_file (int pid, int old_nice,int new_nice, char *procname);
void get_time();
void flush_logs();
int open_log_file(char *filename);
#endif
