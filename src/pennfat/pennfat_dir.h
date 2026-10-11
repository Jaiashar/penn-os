/** @file
 * Kernel-level functions for working with directory entries
 */

#ifndef PENNFAT_DIR_H
#define PENNFAT_DIR_H

#include <sys/types.h>
#include <time.h>
#include "../os/pennfat.h"

/**
 * Directory entry, in the layout stored on disk
 */
typedef struct DirEntry {
    char name[32];  /**< Null-terminated file name */
    uint32_t size;  /**< Number of bytes in file */
    uint16_t block; /**< First block number */
    uint8_t type;   /**< File type (see filetype_t) */
    uint8_t perm;   /**< Permission bits (see fileperm_t) */
    time_t mtime;   /**< Modification time */
    char reserved[16];
} direntry_t;

/**
 * Get details of a single file or a list of files.
 *
 * @param filename  Name of the file to get details for, or if NULL, all files
 *                  in the current directory
 *
 * @return A single filedetails_t if filename was provided, or a linked list of
 *         filedetails_t if not. On error, returns NULL and sets p_errno. Note
 *         that NULL can be returned without an error if there are no files to
 *         list; check by setting p_errno to zero before calling. Non-NULL
 *         returns must be freed with k_freels().
 *
 * **Errors:**
 * - PINVAL filename is an empty string
 * - PNOFILE filename does not exist
 * - NOMEM Could not allocate memory
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
filedetails_t* k_ls(const char *filename);

/**
 * Free the return value from k_ls().
 *
 * @param fdetails  The return value from k_ls() to free. NULL is allowed.
 *
 * This call cannot return errors.
 */
void k_freels(filedetails_t *fdetails);

/**
 * Delete the named file.
 *
 * @param fname         Name of file to delete
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL filename is NULL or an empty string
 * - PNOFILE filename does not exist
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int k_unlink(const char *fname);

/**
 * Change permissions of the named file.
 *
 * @param fname Name of the file whose permissions to change
 * @param perm  Permissions to assign
 * @param mode  F_CHMOD_SET, F_CHMOD_ADD, or F_CHMOD_REMOVE
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL filename is NULL or an empty string, perm not a fileperm_t,
 *   or mode not a chmodmode_t
 * - PNOFILE filename does not exist
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int k_chmod(const char *fname, fileperm_t perm, chmodmode_t mode);

/** @internal
 * Count the number of directory entries in the root directory, until and not
 * including the last name[0] == 0 entry. Store the result for use within this
 * module.
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int count_dirents(void);

/** @internal
 * Iterate over directory entries in the root directory. Caller is responsible
 * for stopping at the last dirent.
 *
 * @param[in,out] block     Current block number. Updated if crossing a block
 *                          boundary.
 * @param[in,out] offset    Current offset in the directory. Updated with each
 *                          read.
 * @param[out]    dirent    The direntry_t to store the next entry in
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int next_dirent(uint16_t *block, uint32_t *offset, direntry_t *dirent);

/** @internal
 * Find the directory entry for the named file and return its offset in the
 * root directory.
 *
 * @param[in]  f_name   Name of the file whose directory entry to fetch
 * @param[out] dirent   Directory entry to store in
 *
 * @return Offset of the directory entry in the root directory, -1 if not
 *         found, or -2 and sets p_errno on error.
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
ssize_t find_dirent(const char *f_name, direntry_t *dirent);

/** @internal
 * Find or allocate an unused directory entry in the root directory.
 *
 * @return Offset to the unused entry, or -1 and sets p_errno on error.
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 * - PFULL The filesystem has no more free blocks
 */
ssize_t find_free_dirent(void);

/** @internal
 * Read a directory entry from the root directory at the given offset
 *
 * @param[in]  offset   Offset to read from
 * @param[out] dirent   direntry_t to store the result in
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int read_dirent(uint32_t offset, direntry_t *dirent);

/** @internal
 * Write the directory entry back to the root directory. The caller is
 * responsible for ensuring the offset is not past the end of the root
 * directory.
 *
 * @param offset    Offset to write at
 * @param dirent    Directory entry to write
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 * - PFULL The filesystem has no more free blocks (generally should not happen
 *   except when called by find_free_dirent())
 */
int write_dirent(uint32_t offset, direntry_t *dirent);

#endif
