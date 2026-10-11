/** @file
 * PennFAT user-level API - The "system calls" that userland programs like
 * the shell use for filesystem interaction
 */

#ifndef PENNFAT_H
#define PENNFAT_H

#include <stdint.h>
#include <time.h>

/**
 * Modes that files can be opened in
 */
typedef enum {
    F_WRITE,    /**< Truncate and open for read and write, create if does not
                     exist */
    F_READ,     /**< Open for read only, error if does not exist */
    F_APPEND    /**< Open for read and write, start at end of file, create if
                     does not exist */
} filemode_t;

/**
 * Allowed values for whence in f_lseek
 */
typedef enum {
    F_SEEK_SET, /**< Offset is absolute */
    F_SEEK_CUR, /**< Offset is relative to the current position */
    F_SEEK_END  /**< Offset is relative to the end of the file */
} whence_t;

/**
 * Allowed values for mode in f_chmod
 */
typedef enum {
    F_CHMOD_SET,    /**< Set the permissions to the given value */
    F_CHMOD_ADD,    /**< Add the given permissions */
    F_CHMOD_REMOVE  /**< Remove the given permissions */
} chmodmode_t;

/**
 * Constants representing the allowed block sizes for PennFAT
 */
typedef enum {
    BS_256,
    BS_512,
    BS_1024,
    BS_2048,
    BS_4096
} fatblocksize_t;

/**
 * File types
 */
typedef enum {
    FTYPE_UNKNOWN,  /**< Unknown file type */
    FTYPE_REGULAR,  /**< Regular file */
    FTYPE_DIR,      /**< Directory */
    FTYPE_LINK      /**< Symbolic link */
} filetype_t;

/**
 * File permissions for directory entires and f_ls()
 */
typedef enum {
    PERM_NONE = 0,  /**< No permissions */
    PERM_X = 1,     /**< Execute permission (not meant to be used alone, but
                         can be ORed with others) */
    PERM_W = 2,     /**< Write permission */
    PERM_R = 4,     /**< Read permission */
    PERM_RX = 5,    /**< Read and execute permissions */
    PERM_RW = 6,    /**< Read and write permissions */
    PERM_RWX = 7    /**< All permissions */
} fileperm_t;

/**
 * File details, returned by f_ls()
 */
typedef struct FileDetails {
    char name[32];  /**< Null-terminated file name */
    uint32_t size;  /**< Number of bytes in file */
    uint16_t block; /**< First block number */
    uint8_t perm;   /**< Permission bits (see fileperm_t) */
    time_t mtime;   /**< Modification time */
    struct FileDetails *next;   /**< Pointer to the next file, or NULL */
} filedetails_t;

/**
 * Create a PennFAT filesystem under the named file. This will erase any
 * previous contents of the file.
 *
 * @param fs_name       Name of the file to create the filesystem in
 * @param blocks        Number of blocks in the FAT, 1-32
 * @param block_size    Size of each block, 0-4 (see fatblocksize_t)
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL fs_name is NULL or empty, blocks is not 1-32, or block_size is not
 *   a valid constant from fatblocksize_t
 * - POPENFAIL Cannot open fs_name from the host
 * - PIOFAIL Error writing to fs_name from the host
 * - PCLOSEFAIL Error closing fs_name from the host
 */
int f_mkfs(const char *fs_name, int blocks, fatblocksize_t block_size);

/**
 * Mount a PennFAT filesystem from the named file.
 *
 * @param fs_name   Name of the filesystem file to mount
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL fs_name is NULL or empty
 * - PMOUNTED A filesystem is already mounted
 * - POPENFAIL Cannot open fs_name from the host
 * - PIOFAIL Error reading or memory mapping the FAT or counting dir entries
 * - PCORRUPT Filesystem is corrupt
 */
int f_mount(const char *fs_name);

/**
 * Unmount the currently mounted filesystem.
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS Filesystem is not currently mounted
 * - PINUSE A file is still open
 * - PIOFAIL Error unmapping the FAT (filesystem is still unmounted)
 * - PCLOSEFAIL Error closing the file from the host (filesystem is still
 *   unmounted)
 */
int f_umount(void);

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
 *         returns must be freed with f_freels().
 *
 * **Errors:**
 * - PINVAL filename is an empty string
 * - PNOFILE filename does not exist
 * - NOMEM Could not allocate memory
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
filedetails_t* f_ls(const char *filename);

/**
 * Free the return value from f_ls().
 *
 * @param fdetails  The return value from f_ls() to free. NULL is allowed.
 *
 * This call cannot return errors.
 */
void f_freels(filedetails_t *fdetails);

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
int f_unlink(const char *fname);

/**
 * Change permissions of the named file.
 *
 * @param fname Name of the file whose permissions to change
 * @param perm  Permissions to set
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
int f_chmod(const char *fname, fileperm_t perm, chmodmode_t mode);

/**
 * Open a file for reading, writing, or appending.
 *
 * If opening for reading: Reading will start at the beginning of the file. If
 * the file does not yet exist, it is an error.
 *
 * If opening for writing: Writing will start at the beginning of the file. If
 * the file already exists, it will be truncated. If not, it will be created.
 * If it exists and some process (including the current one) already has it
 * open for writing or appending, it is an error.
 *
 * If opening for appending: Writing will start at the end of the file and it
 * will not be truncated. If the file does not exist, it will be created. If it
 * does exist and some process (including the current one) already has it open
 * for writing or appending, it is an error.
 *
 * @param fname Name of the file to open
 * @param mode  One of F_WRITE, F_READ, or F_APPEND
 *
 * @return A file descriptor for the new file, or -1 and sets p_errno if
 *         unsuccessful
 *
 * **Errors:**
 * - PINVAL f_name is NULL, empty, or longer than 31 characters, or mode is not
 *   F_READ, F_WRITE, or F_APPEND
 * - PPERM f_name does not have proper permissions
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PNOFILE mode is F_READ and f_name is not the name of an existing file
 * - PWLOCK mode is F_WRITE or F_APPEND and f_name is already open for writing
 * - PFULL File needs to be created but filesystem has no more free blocks
 * - PNOMEM Could not allocate memory
 * - PNOFS Filesystem is not mounted
 * - PCORRUPT Filesystem is corrupt
 */
int f_open(const char *fname, int mode);

/**
 * Close an open file descriptor.
 *
 * Even when this call fails, the file descriptor will still be closed and no
 * further attempts to close it should be made.
 *
 * @param fd    File descriptor to close
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL File descriptor is not valid
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system
 * - PCORRUPT The filesystem is corrupt
 */
int f_close(int fd);

/**
 * Read bytes from an open file descriptor, advancing the file's position. May
 * read less than requested if the end of file is reached or an error occurs.
 *
 * If an error occurs after a partial read, the next f_read will yield the
 * error, as long as it was not transient.
 *
 * @param fd    File descriptor to read from
 * @param n     Number of bytes to read
 * @param buf   Buffer to read bytes into
 *
 * @return Number of bytes read. May be less than requested if an error or EOF
 *         occurs after some bytes have been successfully read. 0 will only be
 *         returned for EOF. If an error occurs and no bytes have been read,
 *         returns -1 and sets p_errno.
 *
 * **Errors:**
 * - PINVAL n is negative or file descriptor is not valid
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the file system before any bytes
 *   could be read
 * - PCORRUPT The filesystem is corrupt
 */
int f_read(int fd, int n, char *buf);

/**
 * Write bytes to an open file descriptor, advancing the file's position,
 * enlarging the size if writing past the end of the file, and updating the
 * file's timestamp. May write less than requested if the filesystem fills up
 * or some other error occurs.
 *
 * If an error occurs after a partial write, the next f_write will yield the
 * error, as long as it was not transient.
 *
 * @param fd    File descriptor to write to
 * @param str   Bytes to write
 * @param n     Number of bytes to write
 *
 * @return Number of bytes written. May be less than requested if an error
 *         occurs (including disk full) after some bytes have been successfully
 *         written. If an error occurs when no bytes have been written, returns
 *         -1 and sets p_errno.
 *
 * **Errors:**
 * - PINVAL n is negative or file descriptor is not valid
 * - PMODE File is not open for writing
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system before any
 *   bytes have been written
 * - PFULL There are no free blocks left and there were no bytes left in the
 *   current block.
 * - PCORRUPT The filesystem is corrupt
 */
int f_write(int fd, const char *str, int n);

/**
 * Update the current position of an open file descriptor.
 *
 * @param fd        File descriptor to update
 * @param offset    Offset relative to whence to set
 * @param whence    Where offset is referenced from (see whence_t)
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL whence is not valid or file descriptor is not valid
 * - PTOOFAR The given offset from whence falls outside the file bounds
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int f_lseek(int fd, int offset, int whence);

#endif
