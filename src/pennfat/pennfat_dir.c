#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "pennfat_fs.h"
#include "pennfat_io.h"
#include "p_errno.h"
#include "pennfat_dir.h"

/**
 * Number of directory entries in the list before the last name[0] == 0 entry
 */
int num_dirents;

int count_dirents(void) {
    uint16_t block = 1;
    uint32_t offset = 0;
    direntry_t dirent;
    num_dirents = 0;
    while (1) {
        if (next_dirent(&block, &offset, &dirent) < 0) {
            return -1;
        }
        if (dirent.name[0] == 0) {
            return 0;
        }
        num_dirents++;
    }
}

int next_dirent(uint16_t *block, uint32_t *offset, direntry_t *dirent) {
    int bytes_read = 0;
    while (bytes_read < sizeof(*dirent)) {
        int result = raw_read(block, offset,
                              ((void *) dirent) + bytes_read,
                              sizeof(*dirent) - bytes_read);
        if (result < 0) {
            return -1;
        }
        bytes_read += result;
    }
    return 0;
}

ssize_t find_dirent(const char *f_name, direntry_t *dirent) {
    uint16_t block = 1;
    uint32_t offset = 0;
    while (1) {
        if (next_dirent(&block, &offset, dirent) < 0) {
            return -2;
        }
        if (dirent->name[0] == 0) {
            return -1;
        }
        if (dirent->name[0] == 1 || dirent->name[0] == 2) {
            continue;
        }
        if (strcmp(f_name, dirent->name) == 0) {
            return offset - sizeof(*dirent);
        }
    }
}

ssize_t find_free_dirent(void) {
    uint16_t block = 1;
    uint32_t offset = 0;
    direntry_t dirent;
    while (1) {
        if (next_dirent(&block, &offset, &dirent) < 0) {
            return -1;
        }
        if (dirent.name[0] == 1) {
            return offset - sizeof(dirent);
        }
        if (dirent.name[0] == 0) {
            // Reached the end of the list. Write a new end entry to extend it.
            if (write_dirent(offset, &dirent) < 0) {
                return -1;
            }
            return offset - sizeof(dirent);
        }
    }
}

int read_dirent(uint32_t offset, direntry_t *dirent) {
    uint16_t block = seek_block(1, offset);
    if (block == 0) {
        return -1;
    }
    if (next_dirent(&block, &offset, dirent) < 0) {
        return -1;
    }
    return 0;
}

int write_dirent(uint32_t offset, direntry_t *dirent) {
    uint16_t block = seek_block(1, offset);
    if (block == 0) {
        return -1;
    }
    if (block == 0xffff) {
        p_set_errno(PCORRUPT);
        return -1;
    }
    int bytes_written = 0;
    while (bytes_written < sizeof(*dirent)) {
        int result = raw_write(NULL, &block, &offset,
                               ((void *) dirent) + bytes_written,
                               sizeof(*dirent) - bytes_written);
        if (result < 0) {
            return -1;
        }
        bytes_written += result;
    }
    return 0;
}

/** @internal
 * Allocate and fill a filedetails_t from the given direntry_t
 *
 * @param dirent    Directory entry to fill the filedetails_t from
 *
 * @return Pointer to the allocated filedetails_t on success, or NULL and sets
 *         p_errno() on failure
 *
 * Errors:
 * - PNOMEM Could not allocate a filedetails_t
 */
static filedetails_t* new_filedetails(direntry_t *dirent) {
    filedetails_t *result = malloc(sizeof(filedetails_t));
    if (result == NULL) {
        p_set_errno(PNOMEM);
        return NULL;
    }
    strcpy(result->name, dirent->name);
    result->size = dirent->size;
    result->block = dirent->block;
    result->perm = dirent->perm;
    result->mtime = dirent->mtime;
    result->next = NULL;
    return result;
}

void k_freels(filedetails_t *fdetails) {
    while (fdetails != NULL) {
        filedetails_t *next = fdetails->next;
        free(fdetails);
        fdetails = next;
    }
}

filedetails_t* k_ls(const char *filename) {
    // Check for empty filename
    if (filename != NULL && filename[0] == '\0') {
        p_set_errno(PINVAL);
        return NULL;
    }

    direntry_t dirent;
    uint16_t block = 1;
    uint32_t offset = 0;
    filedetails_t *result = NULL;
    while (1) {
        if (next_dirent(&block, &offset, &dirent) < 0) {
            k_freels(result);
            return NULL;
        }
        if (dirent.name[0] == 0) {
            // No more files
            if (filename != NULL) {
                p_set_errno(PNOFILE);
                return NULL;
            }
            return result;
        }
        if (dirent.name[0] == 1 || dirent.name[0] == 2) {
            // Deleted file
            continue;
        }
        if (filename != NULL && strcmp(filename, dirent.name) == 0) {
            // Looking for one file and found it
            filedetails_t *fdt = new_filedetails(&dirent);
            if (fdt == NULL) {
                p_set_errno(PNOMEM);
                return NULL;
            }
            return fdt;
        }
        if (filename == NULL) {
            // Listing all files, dirent is valid
            filedetails_t *fdt = new_filedetails(&dirent);
            if (fdt == NULL) {
                k_freels(result);
                p_set_errno(PNOMEM);
                return NULL;
            }
            fdt->next = result;
            result = fdt;
        }
    }
}

int k_unlink(const char *fname) {
    // Check for invalid filename
    if (fname == NULL || fname[0] == '\0') {
        p_set_errno(PINVAL);
        return -1;
    }

    // Fetch directory entry
    direntry_t dirent;
    ssize_t dirent_offset = find_dirent(fname, &dirent);
    if (dirent_offset == -1) {
        p_set_errno(PNOFILE);
        return -1;
    } else if (dirent_offset == -2) {
        return -1;
    }

    // Update directory entry
    int in_use = is_open(dirent.block);
    dirent.name[0] = is_open(dirent.block) ? 2 : 1;
    if (write_dirent(dirent_offset, &dirent) < 0) {
        return -1;
    }

    // Free the blocks
    if (!in_use) {
        if (free_blocks(dirent.block) < 0) {
            if (p_errno() == PINVAL) {
                p_set_errno(PCORRUPT);
            }
            return -1;
        }
    }

    return 0;
}

int k_chmod(const char *fname, fileperm_t perm, chmodmode_t mode) {
    // Check for invalid parameters
    if (fname == NULL || fname[0] == '\0' ||
        perm < 0 || perm > PERM_RWX) {
        p_set_errno(PINVAL);
        return -1;
    }

    // Fetch directory entry
    direntry_t dirent;
    ssize_t dirent_offset = find_dirent(fname, &dirent);
    if (dirent_offset == -1) {
        p_set_errno(PNOFILE);
        return -1;
    } else if (dirent_offset == -2) {
        return -1;
    }

    // Update directory entry
    switch (mode) {
        case F_CHMOD_SET:
            dirent.perm = perm;
            break;
        case F_CHMOD_ADD:
            dirent.perm |= perm;
            break;
        case F_CHMOD_REMOVE:
            dirent.perm &= ~perm;
            break;
        default:
            p_set_errno(PINVAL);
            return -1;
    }
    if (write_dirent(dirent_offset, &dirent) < 0) {
        return -1;
    }

    return 0;
}
