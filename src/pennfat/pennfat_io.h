/** @file
 * Kernel-level functions for opening, closing, reading, writing, and
 * seeking on PennFAT files
 */

#ifndef PENNFAT_IO_H
#define PENNFAT_IO_H

#include "file_handle.h"
#include "../os/pennfat.h"

/**
 * Open a file for reading. Start at the beginning of the file. Error if the
 * file does not exist.
 *
 * The resulting filehandle_t must be closed with k_close().
 *
 * @param f_name    Name of the file to open
 *
 * @return A pointer to an filehandle_t if successful, NULL and sets p_errno if
 *         unsuccessful.
 *
 * **Errors:**
 * - PINVAL f_name is NULL, empty, or longer than 31 characters
 * - PPERM f_name does not have read permissions
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PNOFILE f_name is not the name of an existing file
 * - PNOMEM Could not allocate memory
 * - PNOFS Filesystem is not mounted
 * - PCORRUPT Filesystem is corrupt
 */
filehandle_t* k_open_r(const char *f_name);

/**
 * Open a file for writing. Optionally truncate it first. Start at the end of
 * the file. Create the file if it does not exist.
 *
 * The file's timestamp will be set to the current time if creating or
 * truncating the file.
 *
 * The resulting filehandle_t must be closed with k_close().
 *
 * @param f_name    Name of the file to open
 * @param trunc     If nonzero, the file will be truncated when opening.
 *
 * @return A pointer to an filehandle_t if successful, NULL and sets p_errno if
 *         unsuccessful.
 *
 * **Errors:**
 * - PINVAL f_name is NULL, empty, or longer than 31 characters
 * - PPERM f_name does not have read/write permissions
 * - PIOFAIL Read/write error when trying to access the filesystem
 * - PWLOCK f_name is already open for writing
 * - PNOMEM Could not allocate memory
 * - PFULL Filesystem has no more free blocks
 * - PNOFS Filesystem is not mounted
 * - PCORRUPT Filesystem is corrupt
 */
filehandle_t *k_open_w(const char *f_name, int trunc);

/**
 * Close an open file. If there are other references to this file, its
 * reference count will simply be decremented. Otherwise, the file handle will
 * be fully closed and cleaned up.
 *
 * Even when this call fails, the file handle will still be closed and no
 * further attempts to close it should be made.
 *
 * @param file  Pointer to the filehandle_t to close
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system
 * - PCORRUPT The filesystem is corrupt
 */
int k_close(filehandle_t *file);

/**
 * Read bytes from an filehandle_t, advancing the file's position. May read
 * less than requested if the end of file is reached or an error occurs.
 *
 * If an error occurs after a partial read, the next k_read will yield the
 * error, as long as it was not transient.
 *
 * @param file  File to read from
 * @param n     Number of bytes to read
 * @param buf   Buffer to read bytes into
 *
 * @return Number of bytes read. May be less than requested if an error or EOF
 *         occurs after some bytes have been successfully read. 0 will only be
 *         returned for EOF. If an error occurs and no bytes have been read,
 *         returns -1 and sets p_errno.
 *
 * **Errors:**
 * - PINVAL n is negative
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the file system before any bytes
 *   could be read
 * - PCORRUPT The filesystem is corrupt
 */
int k_read(filehandle_t *file, int n, char *buf);

/**
 * Write bytes to a filehandle_t, advancing the file's position, enlarging the
 * size if writing past the end of the file, and updating the file's timestamp.
 * May write less than requested if the filesystem fills up or some other error
 * occurs.
 *
 * If an error occurs after a partial write, the next k_write will yield the
 * error, as long as it was not transient.
 *
 * @param file  File to write to
 * @param str   Bytes to write
 * @param n     Number of bytes to write
 *
 * @return Number of bytes written. May be less than requested if an error
 *         occurs (including disk full) after some bytes have been successfully
 *         written. If an error occurs when no bytes have been written, returns
 *         -1 and sets p_errno.
 *
 * **Errors:**
 * - PMODE File is not open for writing
 * - PINVAL n is negative
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system before any
 *   bytes have been written
 * - PFULL There are no free blocks left and there were no bytes left in the
 *   current block.
 * - PCORRUPT The filesystem is corrupt
 */
int k_write(filehandle_t *file, const char *str, int n);

/**
 * Update the current position of an filehandle_t.
 *
 * @param file      File to update
 * @param offset    Offset relative to whence to set
 * @param whence    Where offset is referenced from (see whence_t in pennfat.h)
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PINVAL whence is not valid
 * - PTOOFAR The given offset from whence falls outside the file bounds
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the filesystem
 * - PCORRUPT The filesystem is corrupt
 */
int k_lseek(filehandle_t *file, int offset, whence_t whence);

/**
 * Check if any files are currently open.
 *
 * @return Nonzero if yes, zero if no.
 *
 * This call cannot encounter any errors.
 */
int k_fsinuse(void);

/** @internal
 * Check if the file starting at the given block is currently open.
 *
 * @param block First block of the file to check
 *
 * @return Nonzero if open, zero if not open
 */
int is_open(uint16_t block);

#endif
