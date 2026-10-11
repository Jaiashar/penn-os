/** @file
 * Kernel-level functions for creating and working with the file system on a
 * low level (e.g. formatting, mounting, low-level read and write)
 */

#ifndef PENNFAT_FS_H
#define PENNFAT_FS_H

#include <stdint.h>
#include <stddef.h>
#include "../os/pennfat.h"

/**
 * Details of a currently mounted filesystem
 */
typedef struct Filesystem {
    int fd;             /**< File descriptor for the open filesystem */
    uint16_t *fat;      /**< Pointer to the memory mapped FAT */
    size_t block_size;  /**< Block size for this filesystem */
    size_t num_blocks;  /**< Number of data blocks (= FAT entries - 1,
                             or -2 for a max-size fiesystem). Note that actual
                             block indices start at 1, so the max index ==
                             num_blocks! */
} filesystem_t;

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
int k_mkfs(const char *fs_name, int blocks, fatblocksize_t block_size);

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
int k_mount(const char *fs_name);

/**
 * Unmount the currently mounted filesystem.
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful
 *
 * **Errors:**
 * - PNOFS Filesystem is not currently mounted
 * - PIOFAIL Error unmapping the FAT (filesystem is still unmounted)
 * - PCLOSEFAIL Error closing the file from the host (filesystem is still
 *   unmounted)
 */
int k_umount(void);

/** @internal
 * Read n bytes into the buffer starting at the given offset in the given
 * block. The offset and block will be updated to reflect the new position.
 * This is a low-level read: It is up to the caller to ensure it will not read
 * past the end of a file that ends mid-block. However, it will stop reading
 * at the end of the last block in the file (i.e. do a partial read, then read
 * 0 bytes when actually at the end).
 *
 * Error will be returns if and only if no bytes could be read at all. (If a
 * read is only partially successful, the next read attempt will return the
 * error, so long as it was not transient.)
 *
 * @param[in,out] block     Block to start reading in. Will be updated if the
 *                          read passes the end of this block.
 * @param[in,out] offset    Offset to start reading at from the start of the
 *                          file. Will be updated to reflect the new position.
 * @param[out]    buf       Buffer to read into.
 * @param[in]     n         Number of bytes to read.
 *
 * @return Number of bytes read if successful, -1 and sets p_errno if
 *         unsuccessful. Can return less bytes than expected or even 0 if the
 *         end of the last block is reached or there is an error after
 *         some bytes have already been read.
 *
 * **Errors:**
 * - PINVAL n is negative
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read error when trying to access the file system before any bytes
 *   could be read
 * - PCORRUPT The filesystem is corrupt
 */
int raw_read(uint16_t *block, uint32_t *offset, void *buf, int n);

/** @internal
 * Write n bytes from the buffer starting at the given offset in the given
 * block. The offset and block will be updated to reflect the new position.
 * This is a low-level write: It is up to the caller to update the file
 * timestamp and file size.
 *
 * Errors will only be returned if no bytes can be written at all. (If a write
 * is only partially successful, the next write attempt will return the error,
 * so long as it was not transient.)
 *
 * @param[out]    block0    If the offset is 0 and block is 0xffff, it's
 *                          assumed that no blocks have been allocated for this
 *                          file yet. Only in that case, this will be set to
 *                          the first block in the file.
 * @param[in,out] block     Block to start writing in. Will be updated if the
 *                          write passes the end of this block.
 * @param[in,out] offset    Offset to start writing at from the start of the
 *                          file. Will be updated to reflect the new position.
 * @param[out]    buf       Buffer to write from.
 * @param[in]     n         Number of bytes to write.
 *
 * @return Number of bytes written if successful, -1 and sets p_errno if
 *         unsuccessful. Can return less bytes than expected if the disk
 *         reaches full capacity or there is an error after some bytes have
 *         already been written.
 *
 * **Errors:**
 * - PINVAL n is negative
 * - PNOFS A filesystem is not mounted
 * - PIOFAIL Read/write error when trying to access the file system before any
 *   bytes have been written
 * - PFULL There are no free blocks left and there were no bytes left in the
 *   current block.
 * - PCORRUPT The filesystem is corrupt
 */
int raw_write(uint16_t *block0, uint16_t *block, uint32_t *offset,
              const void *buf, int n);

/** @internal
 * Given the first block in a file and an offset, return the block
 * corresponding to that offset. If the offset falls precisely on a block
 * boundary (i.e. offset % fs.block_size == 0), the block that just ended is
 * returned, not the block just starting.
 *
 * The caller is responsible for ensuring the offset is not past the end of the
 * file!
 *
 * @param block     First block in the file
 * @param offset    Offset whose block to fetch
 *
 * @return Block number containing the given offset, or potentially 0xffff if
 *         past the end of the file, or 0 if there is an error.
 *
 * **Errors:**
 * - PINVAL block is 0, 0xffff or outside the FAT
 * - PNOFS A filesystem is not mounted
 * - PCORRUPT The filesystem is corrupt
 */
uint16_t seek_block(uint16_t block, uint32_t offset);

/** @internal
 * Allocate the first unused block number and make the given block point to it.
 * If 0 is given as the block number, just allocate an unused block and return
 * it.
 *
 * Does not verify that a filesystem is actually mounted or that the provided
 * block number is valid and within the FAT!
 *
 * @param block Block number to point at the new block, or 0 if none
 *
 * @return The block number, or 0xffff if there are none left.
 */
uint16_t allocate_block(uint16_t block);

/**
 * Free the given block and all the blocks it points to, effectively freeing
 * the space used by a file.
 *
 * @param block The block whose subsequent blocks should be freed
 *
 * @return 0 if successful, -1 and sets p_errno if unsuccessful.
 *
 * **Errors:**
 * - PINVAL block is 0 or outside the FAT
 * - PNOFS A filesystem is not mounted
 * - PCORRUPT The filesystem is corrupt
 */
int free_blocks(uint16_t block);

#endif
