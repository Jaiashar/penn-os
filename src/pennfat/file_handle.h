/** @file
 * File handle struct, needed by both the scheduler (for the PCB) and PennFAT
 */
#ifndef FILE_HANDLE_H
#define FILE_HANDLE_H

#include <stdint.h>

/**
 * Represents the read/write mode of an open file.
 * Write and append are not distinct (unlike the user level filemode_t) since
 * the only difference is the file position.
 */
typedef enum {
    MODE_READ,  /**< File is open for read only */
    MODE_WRITE  /**< File is open for read/write */
} kfilemode_t;

/**
 * Contains all the details of a currently open file. File descriptors refer to
 * one of these.
 */
typedef struct FileHandle {
    uint32_t position;  /**< Current offset from the start of the file */
    kfilemode_t mode;   /**< Read/write mode of the file */
    uint16_t block;     /**< Block number for the current position */
    uint16_t fat_entry; /**< Index of this file in the FAT (first block) */
    uint32_t dirent;    /**< Offset of the directory entry within the root
                             directory */
    unsigned refcount;  /**< Reference count. Incremented when multiple FDs
                             point to the same file handle (e.g. a process
                             calls p_spawn and inherits copies of FDs from the
                             parent) */
    struct FileHandle *prev; /**< Previous handle in the systemwide list */
    struct FileHandle *next; /**< Next handle in the systemwide list */
} filehandle_t;

/**
 * Increment the reference count for an open file. For example, when a process
 * calls p_spawn, it will inherit copies of the parent's file descriptors, so
 * the reference counts of the attached file handles need to be incremented.
 *
 * This only happens when an existing file descriptor is somehow duplicated,
 * not when f_open is called on a file that happens to already be open.
 *
 * @param file  The file handle to increment
 *
 * This call cannot encounter any errors.
 */
void fh_incref(filehandle_t *file);

/**
 * The scheduler should call this to cleanly clean up an open file handle when
 * a process does not do it itself (e.g. if terminated, or it simply exits
 * without calling f_close on all open files).
 *
 * If the file handle still has other references, the reference count will
 * simply be decremented. Otherwise, it will be fully cleaned up.
 *
 * Context switching should be disabled before calling!
 *
 * @param file  The file handle to decrement/close
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system
 * - PCORRUPT The filesystem is corrupt
 */
int fh_close(filehandle_t *file);

#endif
