#include <unistd.h>
#include "../kernel/scheduler.h"
#include "../pennfat/file_handle.h"
#include "../pennfat/pennfat_fs.h"
#include "../pennfat/pennfat_dir.h"
#include "../pennfat/pennfat_io.h"
#include "pennfat.h"
#include "p_errno.h"

//TODO Are k_set_kernel_handler() and k_set_user_handler() the right function
// names?

int f_mkfs(const char *fs_name, int blocks, fatblocksize_t block_size) {
    return k_mkfs(fs_name, blocks, block_size);
}

int f_mount(const char *fs_name) {
    k_set_kernel_handler();
    int result = k_mount(fs_name);
    k_set_user_handler();
    return result;
}

int f_umount(void) {
    k_set_kernel_handler();
    if (k_fsinuse()) {
        p_set_errno(PINUSE);
        k_set_user_handler();
        return -1;
    }
    int result = k_umount();
    k_set_user_handler();
    return result;
}

filedetails_t* f_ls(const char *filename) {
    k_set_kernel_handler();
    filedetails_t *result = k_ls(filename);
    k_set_user_handler();
    return result;
}

void f_freels(filedetails_t *fdetails) {
    k_freels(fdetails);
}

int f_unlink(const char *fname) {
    k_set_kernel_handler();
    int result = k_unlink(fname);
    k_set_user_handler();
    return result;
}

int f_chmod(const char *fname, fileperm_t perm, chmodmode_t mode) {
    k_set_kernel_handler();
    int result = k_chmod(fname, perm, mode);
    k_set_user_handler();
    return result;
}

int f_open(const char *fname, int mode) {
    k_set_kernel_handler();
    filehandle_t *fh;
    switch (mode) {
        case F_READ:
            fh = k_open_r(fname);
            break;
        case F_WRITE:
            fh = k_open_w(fname, 1);
            break;
        case F_APPEND:
            fh = k_open_w(fname, 0);
            break;
        default:
            p_set_errno(PINVAL);
            k_set_user_handler();
            return -1;
    }
    if (fh == NULL) {
        k_set_user_handler();
        return -1;
    }

    //TODO Include for this and change if necessary
    int fd = k_set_fd(fh);
    if (fd < 0) {
        k_close(fh);
        k_set_user_handler();
        return -1;
    }
    k_set_user_handler();
    return fd;
}

int f_close(int fd) {
    k_set_kernel_handler();
    fhref_t *fh = k_clear_fd(fd);
    if (fh == NULL || fh->handle == NULL) {
        p_set_errno(PINVAL);
        k_set_user_handler();
        return -1;
    }

    int result = k_close(fh->handle);
    k_set_user_handler();
    return result;
}

int f_read(int fd, int n, char *buf) {
    k_set_kernel_handler();
    //TODO Include for this and change if necessary
    fhref_t *fh = k_get_fd(fd);
    if (fh == NULL) {
        p_set_errno(PINVAL);
        k_set_user_handler();
        return -1;
    }
    int result;
    if (fh->special_type == STDIN) {
        // Re-enable interrupts since read(2) will block.
        k_set_user_handler();
        result = read(STDIN_FILENO, buf, n);
        if (result < 0) {
            p_set_errno(PIOFAIL);
        }

        return result;
    } else if (fh->special_type == STDOUT) {
        result = -1;
        p_set_errno(PINVAL);
    } else {
        result = k_read(fh->handle, n, buf);
    }

    k_set_user_handler();
    return result;
}

int f_write(int fd, const char *str, int n) {
    k_set_kernel_handler();
    //TODO Include for this and change if necessary
    fhref_t *fh = k_get_fd(fd);
    if (fh == NULL) {
        p_set_errno(PINVAL);
        k_set_user_handler();
        return -1;
    }
    int result;
    if (fh->special_type == STDOUT) {
        // Re-enable interrupts since write(2) will block.
        k_set_user_handler();
        result = write(STDOUT_FILENO, str, n);
        if (result < 0) {
            p_set_errno(PIOFAIL);
        }

        return result;
    } else if (fh->special_type == STDIN) {
        result = -1;
        p_set_errno(PINVAL);
    } else {
        result = k_write(fh->handle, str, n);
    }
    k_set_user_handler();
    return result;
}

int f_lseek(int fd, int offset, int whence) {
    k_set_kernel_handler();
    fhref_t *fh = k_get_fd(fd);
    if (fh == NULL) {
        p_set_errno(PINVAL);
        k_set_user_handler();
        return -1;
    }
    int result;
    if (fh->special_type != NONE) {
        result = -1;
        p_set_errno(PINVAL);
    } else {
        result = k_lseek(fh->handle, offset, whence);
    }
    k_set_user_handler();
    return result;
}
