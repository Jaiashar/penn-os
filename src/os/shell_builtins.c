#include "shell_builtins.h"
#include "process.h"
#include "stress.h"
#include "../kernel/process_state.h"
#include "../kernel/kernel_state.h"
#include "../shell/user_interaction.h"
#include "pennfat.h"
#include "p_errno.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static size_t get_argc(char **argv) {
    size_t i;
    for (i = 1; argv[i] != NULL; i++);
    return i;
}

static char ** getArgCopy(char **argv) {
    int argc=0;
    while(argv[argc] != NULL) {
        argc++;
    }

    char ** argCopy = malloc((argc + 1) * sizeof(char *));
    for(int i =0; i< argc; i++) {
        argCopy[i] = malloc((strlen(argv[i]) + 1) * sizeof(char));
        strcpy(argCopy[i], argv[i]);
    }
    argCopy[argc] = (char *)0;
    return argCopy;
}

static void copy_helper(int source, int dest) {
    char buf[MAX_LINE_LENGTH];
    ssize_t buf_bytes = 0, bytes_in, bytes_out;
    while (1) {
        // Read
        bytes_in = f_read(source,
                          MAX_LINE_LENGTH - buf_bytes,
                          buf + buf_bytes);

        if (bytes_in < 0) {
            p_perror("f_read");
            return;
        }

        buf_bytes += bytes_in;
        if (buf_bytes == 0) {
            // Success
            return;
        }

        // Write
        bytes_out = f_write(dest, buf, buf_bytes);
        if (bytes_out < 0) {
            p_perror("f_write");
            return;
        }

        // Shift written bytes out of buffer
        memmove(buf, buf + bytes_out, buf_bytes - bytes_out);
        buf_bytes -= bytes_out;
    }
}

void s_cat(char **args) {
    // Some argument parsing and validation
    args++;
    int end_input_arg;
    for (end_input_arg = 0; args[end_input_arg] != NULL; end_input_arg++) {
        if (strcmp(args[end_input_arg], "-w") == 0 ||
            strcmp(args[end_input_arg], "-a") == 0) {
            break;
        }
    }
    if (args[end_input_arg] != NULL && args[end_input_arg + 1] == NULL) {
        write_check_format(F_STDOUT_NO, "cat: -w or -a requires an argument\n");
        return;
    }
    if (args[end_input_arg] != NULL && args[end_input_arg + 2] != NULL) {
        write_check_format(F_STDOUT_NO, "cat: -w FILENAME or -a FILENAME must be last argument\n");
        return;
    }

    // Setup output
    int dst;
    char *dstf = NULL;
    int truncate = 0;
    if (args[end_input_arg] == NULL) {
        dst = F_STDOUT_NO;
    } else {
        truncate = (args[end_input_arg][1] == 'w');
        dstf = args[end_input_arg + 1];
        if (truncate) {
            dst = f_open(dstf, F_WRITE);
        } else {
            dst = f_open(dstf, F_APPEND);
        }

        if (dst == -1) {
            p_perror("f_open");
            return;
        }
    }

    if (end_input_arg == 0) {
        copy_helper(F_STDIN_NO, dst);
    } else {
        // Input is files
        // Detect matching append file
        if (dstf != NULL && truncate == 0) {
            for (int i = 0; i < end_input_arg; i++) {
                if (strcmp(args[i], dstf) == 0) {
                    write_check_format(F_STDOUT_NO,
                                       "cat: Cannot append to one of the input files\n");
                    f_close(dst);
                    return;
                }
            }
        }

        // Actual copying
        for (int i = 0; i < end_input_arg; i++) {
            int src = f_open(args[i], F_READ);
            if (src == -1) {
                p_perror("f_open");
                break;
            }

            copy_helper(src, dst);
            if (f_close(src) < 0) {
                p_perror("f_close");
                break;
            }
        }
    }



    if (dst != F_STDOUT_NO && f_close(dst) < 0) {
        p_perror("f_close");
        return;
    }
}

void s_sleep(char ** argv) {
  if(argv[0] == NULL || argv[1] == NULL) {
    write_check_format(F_STDOUT_NO, "sleep: syntax is sleep [seconds]\n");
    return;
  }
  int time = atoi(argv[1]);
  if(argv[2] != NULL) {
    write_check_format(F_STDOUT_NO, "sleep: syntax is sleep [seconds]\n");
    return;
  }
  free(argv[0]);
  free(argv[1]);
  free(argv);
  // p_sleep is defined for clock ticks, so multiply by 10
  p_sleep(time * 10);
}

void s_busy(char **argv) {
    while (1) ;
}

void s_echo(char **argv) {
    for (argv++; *argv != NULL; argv++) {
        write_check(F_STDOUT_NO, *argv);
        write_check(F_STDOUT_NO, " ");
    }

    write_check(F_STDOUT_NO, "\n");
}

void s_ls(char **argv) {
    p_set_errno(0);
    char *fname = NULL;
    if (get_argc(argv) > 1) {
        fname = argv[1];
    }

    filedetails_t *result = f_ls(fname);
    if (result == NULL && p_errno() != 0) {
        p_perror("f_ls");
        return;
    }

    for (filedetails_t *fdt = result; fdt != NULL; fdt = fdt->next) {
        char perms[4] = {0};
        perms[0] = (fdt->perm & PERM_R) ? 'r' : '-';
        perms[1] = (fdt->perm & PERM_W) ? 'w' : '-';
        perms[2] = (fdt->perm & PERM_X) ? 'x' : '-';

        char tm_str[13];
        if (strftime(tm_str, 13, "%b %e %H:%M", localtime(&fdt->mtime)) == 0) {
            write_check_format(F_STDOUT_NO, "ls: time string too long\n");
            break;
        }

        write_check_format(F_STDOUT_NO, "%5d -%s %9d %12s %s\n",
                           fdt->block, perms, fdt->size, tm_str, fdt->name);
    }

    f_freels(result);
}

void s_touch(char **args) {
    for (args++; *args != NULL; args++) {
        int fd = f_open(*args, F_APPEND);
        if (fd == -1) {
            p_perror("f_open");
            continue;
        }
        int result = f_write(fd, "", 0);
        if (result < 0) {
            p_perror("f_write");
            f_close(fd);
            continue;
        }
        result = f_close(fd);
        if (result < 0) {
            p_perror("f_close");
            continue;
        }
    }
}

static void copy_fd(char **args, char **o_srcf) {
    if (args[1] == NULL) {
        write_check(F_STDOUT_NO, "mv: Too few arguments\n");
        return;
    }

    char *srcf = args[1];
    if (o_srcf != NULL) {
        *o_srcf = srcf;
    }

    if (args[2] == NULL) {
        write_check(F_STDOUT_NO, "mv: Too few arguments\n");
        return;
    }
    char *dstf = args[2];
    if (args[3] != NULL) {
        write_check(F_STDOUT_NO, "mv: Too many arguments\n");
        return;
    }

    int src = f_open(srcf, F_READ);
    if (src == -1) {
        p_perror("f_open");
        return;
    }

    int dst = f_open(dstf, F_WRITE);
    if (dst == -1) {
        p_perror("f_open");
        f_close(src);
        return;
    }

    copy_helper(src, dst);
    int result = f_close(src);
    if (result < 0) {
        p_perror("f_close");
        f_close(dst);
        f_unlink(dstf);
        return;
    }
    result = f_close(dst);
    if (result < 0) {
        p_perror("f_close");
        f_unlink(dstf);
        return;
    }
}

void s_mv(char **args) {
    char *srcf;
    copy_fd(args, &srcf);
    int result = f_unlink(srcf);
    if (result < 0) {
        p_perror("f_unlink");
        return;
    }
}

void s_cp(char **args) {
    copy_fd(args, NULL);
}

void s_rm(char **args) {
    for (args++; *args != NULL; args++) {
        if (f_unlink(*args) < 0) {
            p_perror("f_unlink");
            continue;
        }
    }
}

void s_chmod(char **args) {
    fileperm_t perm;
    chmodmode_t mode;
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "chmod: Too few arguments\n");
        return;
    }
    if (args[1][0] == '+') {
        mode = F_CHMOD_ADD;
    } else if (args[1][0] == '-') {
        mode = F_CHMOD_REMOVE;
    } else {
        fprintf(stderr, "chmod: First argument must be +w/+x/-w/-x\n");
        return;
    }
    if (args[1][1] == 'w') {
        perm = PERM_W;
    } else if (args[1][1] == 'x') {
        perm = PERM_X;
    } else {
        fprintf(stderr, "chmod: First argument must be +w/+x/-w/-x\n");
        return;
    }
    if (args[1][2] != '\0') {
        fprintf(stderr, "chmod: First argument must be +w/+x/-w/-x\n");
        return;
    }

    for (args += 2; *args != NULL; args++) {
        if (f_chmod(*args, perm, mode) < 0) {
            p_perror("k_chmod");
        }
    }
}

pid_t s_nice(char **argv, int in_fd, int out_fd) {
    if(argv[1] == NULL) {
        write_check_format(F_STDOUT_NO,"Too few arguments to nice. Usage: nice priority command [arg]\n");
        return -1;
    }
    int priority = atoi(argv[1]);
    if(argv[2] == NULL) {
        write_check_format(F_STDOUT_NO,"Too few arguments to nice. Usage: nice priority command [arg]\n");
        return -1;
    }
    int pid = run_builtin(&argv[2], in_fd, out_fd);
    if(pid <= 0) {
        write_check_format(F_STDOUT_NO, "Error spawning command %s\n", argv[2]);
        return pid;
    }
    p_nice(pid, priority);
    return pid;
}

void s_ps(char **argv) {
    procinfo_t *proc = p_ps();
    write_check_format(F_STDOUT_NO, "PID\tPPID\tPRI\tSTAT\tCMD\n");
    for (procinfo_t *p = proc; p != NULL; p = p->next) {
        pid_t pid = p->pid;
        pid_t ppid = p->ppid;
        int priority = p->priority;
        char state;
        switch (p->state) {
        case RUNNING:
            state = 'R';
            break;
        case STOPPED:
            state = 'S';
            break;
        case BLOCKED:
            state = 'B';
            break;
        case ZOMBIE:
            state = 'Z';
            break;
        default:
            state = '?';
            break;
        }

        write_check_format(F_STDOUT_NO, "%d\t%d\t%d\t%c\t%s\n", pid, ppid, priority, state, p->cmd);
    }

    free_procinfo(proc);
}

void s_kill(char **argv) {
  free(argv[0]);
  if(argv[1] == NULL) {
    write_check_format(F_STDOUT_NO,"Incorrect command. Usage: kill [ -SIGNAL_NAME ] pid ...\n");
    free(argv);
    return;
  }
  int signal = S_SIGTERM;
  int first_pid_index = 1;
  if(strcmp(argv[1], "-cont") == 0) {
    signal = S_SIGCONT;
    first_pid_index = 2;
    free(argv[1]);
  } else if(strcmp(argv[1], "-term") == 0) {
    signal = S_SIGTERM;
    first_pid_index = 2;
    free(argv[1]);
  }else if(strcmp(argv[1], "-stop") == 0) {
    signal = S_SIGSTOP;
    first_pid_index = 2;
    free(argv[1]);
  } else if(atoi(argv[1]) == 0) {
      write_check_format(F_STDOUT_NO,"Incorrect usage. SIGNAL_NAME must be one of -cont, -term, or -stop \n");
      free(argv[1]);
      free(argv);
      return;
  }

  while(argv[first_pid_index] != NULL) {
    int pid = atoi(argv[first_pid_index]);
    if(pid <= 0)
      write_check_format(F_STDOUT_NO,"pid must be greater than 0.\n");
    else {
        if (p_kill(pid, signal) < 0) {
            p_perror("p_kill");
        }
    }
    free(argv[first_pid_index ++]);
    }
    free(argv);
}

void zombie_child()
{
    // MMMMM Brains...!
    return;
}

void zombify(char **argv)
{
    char *a[] = {"zombie_child", (char *)0};
    p_spawn(zombie_child, a, F_STDIN_NO, F_STDOUT_NO);
    while (1)
        ;
    return;
}

void orphan_child(char **argv)
{
    // Please sir,
    // I want some more
    while (1)
        ;
}

void orphanify(char **argv)
{
    char *a[] = {"orphan_child", (char *)0};
    p_spawn(orphan_child, a, F_STDIN_NO, F_STDOUT_NO);
    return;
}

static int man() {
    write_check(F_STDOUT_NO, "sleep\nzombify\norphanify\nbusy\nhang\nnohang\n");
    write_check(F_STDOUT_NO, "recur\nnice\nnice_pid\nlogout\nkill\nps\nls\ntouch\n");
    write_check(F_STDOUT_NO, "cat\necho\nmv\ncp\nrm\nchmod\n");
    return 0;
}

pid_t run_builtin(char **argv, int in_fd, int out_fd) {
    if(argv == NULL) return -1;
    if(argv[0] == NULL) return -1;

    if(strcmp(argv[0], "sleep") == 0) {
        return p_spawn(s_sleep, getArgCopy(argv), in_fd, out_fd);
    }
    if(strcmp(argv[0], "zombify") == 0) {
        return p_spawn(zombify, argv, in_fd, out_fd);
    }
    if(strcmp(argv[0], "orphanify") == 0) {
        return p_spawn(orphanify, argv, in_fd, out_fd);
    }
    if(strcmp(argv[0], "busy") == 0) {
        return p_spawn(s_busy, argv, in_fd, out_fd);
    }
    if (strcmp(argv[0], "hang") == 0) {
        hang();
        return -1;
    }
    if (strcmp(argv[0], "nohang") == 0) {
        nohang();
        return -1;
    }
    if (strcmp(argv[0], "recur") == 0) {
        recur();
        return -1;
    }
    if(strcmp(argv[0], "nice") == 0) {
        return s_nice(argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "nice_pid") == 0) {
        if(argv[1] == NULL || argv[2] == NULL || argv[3] != NULL) {
            write_check_format(F_STDOUT_NO,"Incorrect command. Usage: nice_pid priority pid\n");
            return -1;
        }
        int priority = atoi(argv[1]);
        pid_t pid = atoi(argv[2]);
        p_nice(pid, priority);
        return -1;
    }

    if (strcmp(argv[0], "logout") == 0) {
        p_exit();
        return -1;
    }

    if (strcmp(argv[0], "kill") == 0) {
        return p_spawn(s_kill, getArgCopy(argv), in_fd, out_fd);
    }

    if (strcmp(argv[0], "ps") == 0) {
        return p_spawn(s_ps, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "ls") == 0) {
        return p_spawn(s_ls, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "touch") == 0) {
        return p_spawn(s_touch, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "cat") == 0) {
        return p_spawn(s_cat, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "echo") == 0) {
        return p_spawn(s_echo, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "mv") == 0) {
        return p_spawn(s_mv, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "cp") == 0) {
        return p_spawn(s_cp, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "rm") == 0) {
        return p_spawn(s_rm, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "chmod") == 0) {
        return p_spawn(s_chmod, argv, in_fd, out_fd);
    }

    if (strcmp(argv[0], "man") == 0) {
        man();
        return -1;
    }

    write_check_format(F_STDOUT_NO, "%s not supported\n", argv[0]);
    return -1;
}
