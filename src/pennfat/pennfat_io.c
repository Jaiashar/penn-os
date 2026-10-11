#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include "file_handle.h"
#include "../os/pennfat.h"
#include "pennfat_fs.h"
#include "pennfat_dir.h"
#include "p_errno.h"
#include "pennfat_io.h"

/**
 * Systemwide linked list of file handles
 */
static filehandle_t *files = NULL;

filehandle_t* k_open_r(const char *f_name) {
    // Check parameter validity
    if (f_name == NULL || f_name[0] == '\0' || strlen(f_name) > 31) {
        p_set_errno(PINVAL);
        return NULL;
    }

    // Look up this file in the root directory
    direntry_t dirent;
    ssize_t dirent_offset = find_dirent(f_name, &dirent);
    if (dirent_offset == -2) {
        return NULL;
    } else if (dirent_offset == -1) {
        p_set_errno(PNOFILE);
        return NULL;
    }

    // Check permissons
    if ((dirent.perm & PERM_R) != PERM_R) {
        p_set_errno(PPERM);
        return NULL;
    }

    // Allocate and fill the file handle
    filehandle_t *fh = malloc(sizeof(filehandle_t));
    if (fh == NULL) {
        p_set_errno(PNOMEM);
        return NULL;
    }
    fh->position = 0;
    fh->mode = MODE_READ;
    fh->block = dirent.block;
    fh->fat_entry = dirent.block;
    fh->dirent = dirent_offset;
    fh->refcount = 1;
    fh->prev = NULL;
    fh->next = files;
    if (files != NULL) {
        files->prev = fh;
    }
    files = fh;

    return fh;
}

filehandle_t *k_open_w(const char *f_name, int trunc) {
    // Check parameter validity
    if (f_name == NULL || f_name[0] == '\0' || strlen(f_name) > 31) {
        p_set_errno(PINVAL);
        return NULL;
    }

    // Allocate before we found the dirent so we can check for malloc errors
    // before potentially creating a new dirent
    filehandle_t *fh = malloc(sizeof(filehandle_t));
    if (fh == NULL) {
        p_set_errno(PNOMEM);
        return NULL;
    }

    // Look up this file in the root directory
    direntry_t dirent;
    ssize_t dirent_offset = find_dirent(f_name, &dirent);
    if (dirent_offset == -2) {
        free(fh);
        return NULL;
    } else if (dirent_offset == -1) {
        // File doesn't exist. Create it.
        dirent.block = 0xffff;
        strcpy(dirent.name, f_name);
        dirent.size = 0;
        dirent.type = FTYPE_REGULAR;
        dirent.perm = PERM_RW;
        dirent.mtime = time(NULL);
        // Write the new dirent to the root directory
        dirent_offset = find_free_dirent();
        if (dirent_offset < 0) {
            free(fh);
            return NULL;
        }
        if (write_dirent(dirent_offset, &dirent) < 0) {
            free(fh);
            return NULL;
        }
    } else {
        // File exists, make sure not already open for writing and check
        // permissions
        if ((dirent.perm & PERM_RW) != PERM_RW) {
            p_set_errno(PPERM);
            free(fh);
            return NULL;
        }
        for (filehandle_t *f = files; f != NULL; f = f->next) {
            if (f->mode == MODE_READ) {
                continue;
            }
            if (f->fat_entry == dirent.block) {
                p_set_errno(PWLOCK);
                free(fh);
                return NULL;
            }
        }

        // Truncate if necessary
        if (trunc) {
            dirent.size = 0;
            dirent.mtime = time(NULL);
            // Free all the old blocks
            if (free_blocks(dirent.block) < 0) {
                free(fh);
                return NULL;
            }
            dirent.block = 0xffff;
            // Write the dirent back
            if (write_dirent(dirent_offset, &dirent) < 0) {
                free(fh);
                return NULL;
            }
        }
    }

    fh->position = dirent.size;
    fh->mode = MODE_WRITE;
    fh->block = seek_block(dirent.block, dirent.size);
    if (fh->block == 0) {
        // In a perfect world, we should undo the dirent creation/modification
        // we already did since the call failed, but the only way we will get
        // here is if the filesystem is corrupt, so there's really no need.
        if (p_errno() == PINVAL) {
            p_set_errno(PCORRUPT);
        }
        free(fh);
        return NULL;
    }
    fh->fat_entry = dirent.block;
    fh->dirent = dirent_offset;
    fh->refcount = 1;
    fh->prev = NULL;
    fh->next = files;
    if (files != NULL) {
        files->prev = fh;
    }
    files = fh;

    return fh;
}

int is_open(uint16_t block) {
    for (filehandle_t *f = files; f != NULL; f = f->next) {
        if (f->fat_entry == block) {
            return 1;
        }
    }
    return 0;
}

int k_close(filehandle_t *file) {
    // First, decrement the refcount
    file->refcount--;
    if (file->refcount > 0) {
        // Still other copies open. Nothing left to do.
        return 0;
    }

    // Remove the file handle from the open file list
    if (file->prev == NULL) {
        files = file->next;
    } else {
        file->prev->next = file->next;
    }
    if (file->next != NULL) {
        file->next->prev = file->prev;
    }

    // Check if the file is marked for deletion.
    direntry_t dirent;
    if (read_dirent(file->dirent, &dirent) < 0) {
        free(file);
        return -1;
    }
    if (dirent.name[0] == 2 && !is_open(dirent.block)) {
        // It is, and there are no other open copies. Delete it for good.
        dirent.name[0] = 1;
        if (free_blocks(dirent.block) < 0) {
            if (p_errno() == PINVAL) {
                p_set_errno(PCORRUPT);
            }
            write_dirent(file->dirent, &dirent);
            free(file);
            return -1;
        }
        if (write_dirent(file->dirent, &dirent) < 0) {
            free(file);
            return -1;
        }
    }

    free(file);
    return 0;
}

int k_read(filehandle_t *file, int n, char *buf) {
    // Fetch current file size
    direntry_t dirent;
    if (read_dirent(file->dirent, &dirent) < 0) {
        return -1;
    }

    // Check and adjust n for file size
    if (n < 0) {
        p_set_errno(PINVAL);
        return -1;
    }
    if (n + file->position > dirent.size) {
        n = dirent.size - file->position;
        if (n < 0) {
            n = 0;
        }
    }

    // Do the actual read
    int bytes_read = 0;
    while (n > 0) {
        int result = raw_read(&file->block, &file->position, buf, n);
        if (result < 0) {
            if (bytes_read > 0) {
                return bytes_read;
            } else {
                return -1;
            }
        } else if (result == 0) {
            // Position is past the end of the allocated blocks for this file.
            // This should not happen.
            p_set_errno(PINTERNAL);
            return -1;
        }
        bytes_read += result;
        buf += result;
        n -= result;
    }
    return bytes_read;
}

int k_write(filehandle_t *file, const char *str, int n) {
    // Check argument validity and file mode
    if (n < 0) {
        p_set_errno(PINVAL);
        return -1;
    }
    if (file->mode != MODE_WRITE) {
        p_set_errno(PMODE);
        return -1;
    }

    // Get the current directory entry now, so we can return an error before
    // writing any bytes if necessary
    direntry_t dirent;
    if (read_dirent(file->dirent, &dirent) < 0) {
        return -1;
    }

    // Do the actual write
    int bytes_written = 0;
    while (n > 0) {
        uint16_t block0 = 0xffff;
        int result = raw_write(&block0, &file->block, &file->position, str, n);
        if (file->fat_entry == 0xffff) {
            file->fat_entry = block0;
            dirent.block = block0;
        }
        if (result < 0) {
            if (bytes_written > 0) {
                break;
            } else {
                return -1;
            }
        } else if (result == 0) {
            // Position is past the end of the allocated blocks for this file.
            // This should not happen.
            p_set_errno(PINTERNAL);
            return -1;
        }
        bytes_written += result;
        str += result;
        n -= result;
    }

    // Update the directory entry
    dirent.mtime = time(NULL);
    if (dirent.size < file->position) {
        dirent.size = file->position;
    }
    if (write_dirent(file->dirent, &dirent) < 0) {
        // There's not a great option here... The write was successful, but we
        // may potentially have an invalid size in the directory entry. If we
        // return an error, the caller is in an inconsistent state. If not, the
        // filesystem is in an inconsistent state. Luckily, it should be very
        // unlikely we'll ever have a successful data write followed by a
        // failed dirent write.
        // So, just return the error from write_dirent().
        return -1;
    }

    return bytes_written;
}

int k_lseek(filehandle_t *file, int offset, whence_t whence) {
    // Fetch current file size
    direntry_t dirent;
    if (read_dirent(file->dirent, &dirent) < 0) {
        return -1;
    }

    // Calculate and validate new position
    int32_t pos;
    switch (whence) {
        case F_SEEK_SET:
            pos = offset;
            break;
        case F_SEEK_CUR:
            pos = file->position + offset;
            break;
        case F_SEEK_END:
            pos = dirent.size + offset;
            break;
        default:
            p_set_errno(PINVAL);
            return -1;
    }
    if (pos < 0 || pos > dirent.size) {
        p_set_errno(PTOOFAR);
        return -1;
    }

    // Update position
    uint16_t block = seek_block(dirent.block, pos);
    if (block == 0) {
        if (p_errno() == PINVAL) {
            p_set_errno(PCORRUPT);
        }
        return -1;
    }
    file->block = block;
    file->position = pos;
    return 0;
}

int k_fsinuse(void) {
    return (files != NULL);
}
