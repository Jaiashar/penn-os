#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "pennfat_fs.h"
#include "pennfat_dir.h"
#include "pennfat_io.h"
#include "file_handle.h"
#include "p_errno.h"

#define MAX_LINE_LENGTH 4096

/**
 * Copy the contents of one file to another. Display any errors that occur.
 *
 * There are two options each for the source and dest: a filehandle_t and a
 * fd from the host system. The filehandle_t takes precedence, but if it is
 * NULL, the fd will be used.
 *
 * @param source    Source filehandle_t
 * @param source_fd Source file descriptor from the host
 * @param dest      Destination filehandle_t
 * @param dest_fd   Destination file descriptor from the host
 */
void copy_helper(filehandle_t *source, int source_fd,
                 filehandle_t *dest, int dest_fd) {
    char buf[MAX_LINE_LENGTH];
    ssize_t buf_bytes = 0, bytes_in, bytes_out;
    while (1) {
        // Read
        if (source != NULL) {
            bytes_in = k_read(source,
                              MAX_LINE_LENGTH - buf_bytes,
                              buf + buf_bytes);
            if (bytes_in < 0) {
                p_perror("k_read");
                return;
            }
        } else {
            bytes_in = read(source_fd,
                            buf + buf_bytes,
                            MAX_LINE_LENGTH - buf_bytes);
            if (bytes_in < 0) {
                perror("read");
                return;
            }
        }
        buf_bytes += bytes_in;
        if (buf_bytes == 0) {
            // Success
            return;
        }

        // Write
        if (dest != NULL) {
            bytes_out = k_write(dest, buf, buf_bytes);
            if (bytes_out < 0) {
                p_perror("k_write");
                return;
            }
        } else {
            bytes_out = write(dest_fd, buf, buf_bytes);
            if (bytes_out < 0) {
                perror("write");
                return;
            }
        }

        // Shift written bytes out of buffer
        memmove(buf, buf + bytes_out, buf_bytes - bytes_out);
        buf_bytes -= bytes_out;
    }
}

/**
 * Execute a mkfs command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_mkfs(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mkfs: Too few arguments\n");
        return;
    }
    char *fname = args[1];
    if (args[2] == NULL) {
        fprintf(stderr, "mkfs: Too few arguments\n");
        return;
    }
    int blocks = strtol(args[2], NULL, 0);
    if (args[3] == NULL) {
        fprintf(stderr, "mkfs: Too few arguments\n");
        return;
    }
    int block_size = strtol(args[3], NULL, 0);
    if (args[4] != NULL) {
        fprintf(stderr, "mkfs: Too many arguments\n");
    }
    if (k_mkfs(fname, blocks, block_size) < 0) {
        p_perror("k_mkfs");
    }
}

/**
 * Execute a mount command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_mount(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mount: Too few arguments\n");
        return;
    }
    char *fname = args[1];
    if (args[2] != NULL) {
        fprintf(stderr, "mount: Too many arguments\n");
        return;
    }
    if (k_mount(fname) < 0) {
        p_perror("k_mount");
    }
}

/**
 * Execute a umount command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_umount(char **args) {
    if (args[1] != NULL) {
        fprintf(stderr, "umount: Too many arguments\n");
        return;
    }
    // Note: k_umount does not include the check for open files that f_umount
    // has, but it's not possible to have persistent open files in standalone
    // PennFAT anyway, so it's not a problem.
    if (k_umount() < 0) {
        p_perror("k_umount");
    }
}

/**
 * Execute a touch command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_touch(char **args) {
    for (args++; *args != NULL; args++) {
        filehandle_t *f = k_open_w(*args, 0);
        if (f == NULL) {
            p_perror("k_open_w");
            continue;
        }
        int result = k_write(f, "", 0);
        if (result < 0) {
            p_perror("k_write");
            k_close(f);
            continue;
        }
        result = k_close(f);
        if (result < 0) {
            p_perror("k_close");
            continue;
        }
    }
}

/**
 * Execute a mv command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_mv(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mv: Too few arguments\n");
        return;
    }
    char *srcf = args[1];
    if (args[2] == NULL) {
        fprintf(stderr, "mv: Too few arguments\n");
        return;
    }
    char *dstf = args[2];
    if (args[3] != NULL) {
        fprintf(stderr, "mv: Too many arguments\n");
        return;
    }

    filehandle_t *src = k_open_r(srcf);
    if (src == NULL) {
        p_perror("k_open_r");
        return;
    }
    filehandle_t *dst = k_open_w(dstf, 1);
    if (dst == NULL) {
        p_perror("k_open_w");
        k_close(src);
        return;
    }
    copy_helper(src, 0, dst, 0);
    int result = k_close(src);
    if (result < 0) {
        p_perror("k_close");
        k_close(dst);
        k_unlink(dstf);
        return;
    }
    result = k_close(dst);
    if (result < 0) {
        p_perror("k_close");
        k_unlink(dstf);
        return;
    }
    result = k_unlink(srcf);
    if (result < 0) {
        p_perror("k_unlink");
        return;
    }
}

/**
 * Execute an rm command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_rm(char **args) {
    for (args++; *args != NULL; args++) {
        if (k_unlink(*args) < 0) {
            p_perror("f_unlink");
            continue;
        }
    }
}

/**
 * Execute a cat command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_cat(char **args) {
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
        fprintf(stderr, "cat: -w or -a requires an argument\n");
        return;
    }
    if (args[end_input_arg] != NULL && args[end_input_arg + 2] != NULL) {
        fprintf(stderr, "cat: -w FILENAME or -a FILENAME must be last argument\n");
        return;
    }

    // Setup output
    filehandle_t *dst;
    char *dstf = NULL;
    int truncate = 0;
    if (args[end_input_arg] == NULL) {
        dst = NULL;
    } else {
        truncate = (args[end_input_arg][1] == 'w');
        dstf = args[end_input_arg + 1];
        dst = k_open_w(dstf, truncate);
        if (dst == NULL) {
            p_perror("k_open_w");
            return;
        }
    }

    if (end_input_arg == 0) {
        // Input is stdin
        copy_helper(NULL, 0, dst, 1);
    } else {
        // Input is files
        // Detect matching append file
        if (dstf != NULL && truncate == 0) {
            for (int i = 0; i < end_input_arg; i++) {
                if (strcmp(args[i], dstf) == 0) {
                    fprintf(stderr,
                            "cat: Cannot append to one of the input files\n");
                    k_close(dst);
                    return;
                }
            }
        }
        // Actual copying
        for (int i = 0; i < end_input_arg; i++) {
            filehandle_t *src = k_open_r(args[i]);
            if (src == NULL) {
                p_perror("k_open_r");
                break;
            }
            copy_helper(src, 0, dst, 1);
            if (k_close(src) < 0) {
                p_perror("k_close");
                break;
            }
        }
    }

    if (dst != NULL && k_close(dst) < 0) {
        p_perror("k_close");
        return;
    }
}

/**
 * Execute a cp command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_cp(char **args) {
    // Some argument parsing and validation
    int i = 1;
    int host_file = 0;  // 0: Both are PennFAT  1: Src is host  2: Dst is host
    if (args[i] == NULL) {
        fprintf(stderr, "cp: Too few arguments\n");
        return;
    } else if (strcmp(args[i], "-h") == 0) {
        host_file = 1;
        i++;
    }
    if (args[i] == NULL) {
        fprintf(stderr, "cp: Too few arguments\n");
        return;
    }
    char *srcf = args[i];
    i++;
    if (args[i] == NULL) {
        fprintf(stderr, "cp: Too few arguments\n");
        return;
    } else if (strcmp(args[i], "-h") == 0) {
        if (host_file) {
            fprintf(stderr, "cp: Only one -h allowed\n");
            return;
        }
        host_file = 2;
        i++;
    }
    if (args[i] == NULL) {
        fprintf(stderr, "cp: Too few arguments\n");
        return;
    }
    char *dstf = args[i];
    i++;
    if (args[i] != NULL) {
        fprintf(stderr, "cp: Too many arguments\n");
        return;
    }

    // Make sure source and dest aren't the same
    if (host_file == 0 && strcmp(srcf, dstf) == 0) {
        fprintf(stderr, "cp: Source and dest are the same\n");
        return;
    }

    // Set up source
    filehandle_t *src = NULL;
    int srcfd = 0;
    if (host_file == 1) {
        srcfd = open(srcf, O_RDONLY);
        if (srcfd < 0) {
            perror("open");
            return;
        }
    } else {
        src = k_open_r(srcf);
        if (src == NULL) {
            p_perror("k_open_r");
            return;
        }
    }

    // Set up dest
    filehandle_t *dst = NULL;
    int dstfd = 0;
    if (host_file == 2) {
        dstfd = open(dstf, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP |
                     S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
        if (dstfd < 0) {
            perror("open");
            if (src == NULL) {
                close(srcfd);
            } else {
                k_close(src);
            }
            return;
        }
    } else {
        dst = k_open_w(dstf, 1);
        if (dst == NULL) {
            p_perror("k_open_w");
            if (src == NULL) {
                close(srcfd);
            } else {
                k_close(src);
            }
            return;
        }
    }

    copy_helper(src, srcfd, dst, dstfd);

    // Close everything
    if (src == NULL) {
        if (close(srcfd) < 0) {
            perror("close");
        }
    } else {
        if (k_close(src) < 0) {
            p_perror("k_close");
        }
    }
    if (dst == NULL) {
        if (close(dstfd) < 0) {
            perror("close");
        }
    } else {
        if (k_close(dst) < 0) {
            p_perror("k_close");
        }
    }
}

/**
 * ls helper - Make a single f_ls call. Display any errors that occur and clean
 * up after.
 *
 * @param fname File to list, or NULL for all files in current directory
 */
void ls_helper(char *fname) {
    p_set_errno(0);
    filedetails_t *result = k_ls(fname);
    if (result == NULL && p_errno() != 0) {
        p_perror("k_ls");
        return;
    }
    for (filedetails_t *fdt = result; fdt != NULL; fdt = fdt->next) {
        char perms[4] = {0};
        perms[0] = (fdt->perm & PERM_R) ? 'r' : '-';
        perms[1] = (fdt->perm & PERM_W) ? 'w' : '-';
        perms[2] = (fdt->perm & PERM_X) ? 'x' : '-';

        char tm_str[13];
        if (strftime(tm_str, 13, "%b %e %H:%M", localtime(&fdt->mtime)) == 0) {
            fprintf(stderr, "ls: time string too long\n");
            break;
        }

        printf("%5d -%s %9d %12s %s\n",
               fdt->block, perms, fdt->size, tm_str, fdt->name);
    }
    k_freels(result);
}

/**
 * Execute an ls command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_ls(char **args) {
    if (args[1] == NULL) {
        ls_helper(NULL);
        return;
    }

    for (args++; *args != NULL; args++) {
        ls_helper(*args);
    }
}

/**
 * Execute a chmod command. Display any errors that occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void ex_chmod(char **args) {
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
        if (k_chmod(*args, perm, mode) < 0) {
            p_perror("k_chmod");
        }
    }
}

/**
 * Execute a command from the provided argument list. Display any errors that
 * occur.
 *
 * @param args  NULL-terminated array of command arguments
 */
void execute(char **args) {
    if (strcmp(args[0], "mkfs") == 0) {
        ex_mkfs(args);
    } else if (strcmp(args[0], "mount") == 0) {
        ex_mount(args);
    } else if (strcmp(args[0], "umount") == 0) {
        ex_umount(args);
    } else if (strcmp(args[0], "touch") == 0) {
        ex_touch(args);
    } else if (strcmp(args[0], "mv") == 0) {
        ex_mv(args);
    } else if (strcmp(args[0], "rm") == 0) {
        ex_rm(args);
    } else if (strcmp(args[0], "cat") == 0) {
        ex_cat(args);
    } else if (strcmp(args[0], "cp") == 0) {
        ex_cp(args);
    } else if (strcmp(args[0], "ls") == 0) {
        ex_ls(args);
    } else if (strcmp(args[0], "chmod") == 0) {
        ex_chmod(args);
    } else {
        fprintf(stderr, "Unknown command: %s\n", args[0]);
    }
}

/**
 * Count the number of arguments in a command string
 *
 * @param cmd The command string
 */
size_t count_args(const char *cmd) {
    size_t count = 0;
    int in_arg = 0;
    for (; *cmd != '\0'; ++cmd) {
        if (*cmd == ' ' || *cmd == '\t') {
            in_arg = 0;
        } else if (in_arg == 0) {
            count++;
            in_arg = 1;
        }
    }
    return count;
}

/**
 * Split a command string into a list of arguments. Modifies the original
 * string and returns a NULL-terminated array of arguments.
 *
 * Display an error and terminate the program if malloc fails.
 *
 * @param cmd The command string to tokenize
 *
 * @return NULL-terminated array of arguments. Must be freed when no longer
 *         needed.
 */
char ** tokenize_args(char *cmd) {
    size_t arg_count = count_args(cmd);
    char **args = malloc((arg_count + 1) * sizeof(char *));
    if (args == NULL) {
        perror("malloc");
    }
    args[0] = strtok(cmd, " \t\n");
    for (size_t i = 1; args[i - 1] != NULL; ++i) {
        args[i] = strtok(NULL, " \t\n");
    }
    return args;
}

int main(int argc, char **argv) {
    while (1) {
        printf("pennfat> ");
        char cmd[MAX_LINE_LENGTH];
        if (!fgets(cmd, MAX_LINE_LENGTH, stdin)) {
            if (feof(stdin)) {
                printf("\n");
                exit(EXIT_SUCCESS);
            }
            fprintf(stderr, "Could not read command\n");
            exit(EXIT_FAILURE);
        }

        char **args = tokenize_args(cmd);
        if (args[0] == NULL) {
            // No arguments given
            free(args);
            continue;
        }

        execute(args);
        free(args);
    }
}
