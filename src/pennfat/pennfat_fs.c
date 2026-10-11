#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "p_errno.h"
#include "pennfat_fs.h"
#include "pennfat_dir.h"
#include "../os/pennfat.h"

/**
 * Stores the currently mounted filesystem
 */
static filesystem_t fs = {.fd = -1};

int k_mkfs(const char *fs_name, int blocks, fatblocksize_t block_size) {
    // Check parameter validity
    if (fs_name == NULL || fs_name[0] == '\0' ||
        blocks < 1 || blocks > 32) {
        p_set_errno(PINVAL);
        return -1;
    }

    // Calculate contents
    uint16_t fat_info = blocks << 8 | block_size;
    size_t bs_bytes;
    switch (block_size) {
        case BS_256:
            bs_bytes = 256;
            break;
        case BS_512:
            bs_bytes = 512;
            break;
        case BS_1024:
            bs_bytes = 1024;
            break;
        case BS_2048:
            bs_bytes = 2048;
            break;
        case BS_4096:
            bs_bytes = 4096;
            break;
        default:
            p_set_errno(PINVAL);
            return -1;
    }
    size_t fat_size = bs_bytes * blocks;
    size_t total_bytes = fat_size + bs_bytes * (fat_size / 2 - 1);
    if (bs_bytes == 4096 && blocks == 32) {
        // With a max-size filesystem, remove one block for 0xffff
        total_bytes -= bs_bytes;
    }

    // Write the formatted filesystem
    int fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    if (fd < 0) {
        p_set_errno(POPENFAIL);
        return -1;
    }
    size_t bytes_written = 0;
    uint16_t used = 0xffff;
    char empty[4096] = {0};
    while (bytes_written < total_bytes) {
        int result;
        if (bytes_written < sizeof(uint16_t)) {
            result = write(fd,
                           ((void *) &fat_info) + bytes_written,
                           sizeof(uint16_t) - bytes_written);
        } else if (bytes_written < 2 * sizeof(uint16_t)) {
            result = write(fd,
                           ((void *) &used) + bytes_written - sizeof(uint16_t),
                           2 * sizeof(uint16_t) - bytes_written);
        } else {
            size_t to_write = total_bytes - bytes_written;
            if (to_write > sizeof(empty)) {
                to_write = sizeof(empty);
            }
            result = write(fd, empty, to_write);
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            p_set_errno(PIOFAIL);
            close(fd);
            return -1;
        } else {
            bytes_written += result;
        }
    }
    if (close(fd) < 0) {
        p_set_errno(PCLOSEFAIL);
        return -1;
    }

    return 0;
}

int k_mount(const char *fs_name) {
    // Check parameter validity
    if (fs_name == NULL || fs_name[0] == '\0') {
        p_set_errno(PINVAL);
        return -1;
    }

    // Check if a filesystem is already mounted
    if (fs.fd >= 0) {
        p_set_errno(PMOUNTED);
        return -1;
    }

    // Open and read FAT info entry
    int fd = open(fs_name, O_RDWR);
    if (fd < 0) {
        p_set_errno(POPENFAIL);
        return -1;
    }
    uint16_t fat_info;
    int result = read(fd, &fat_info, sizeof(fat_info));
    if (result < 0) {
        p_set_errno(PIOFAIL);
        close(fd);
        return -1;
    }

    // Obtain block size and check for corruption
    size_t block_size;
    switch (fat_info & 0xff) {
        case BS_256:
            block_size = 256;
            break;
        case BS_512:
            block_size = 512;
            break;
        case BS_1024:
            block_size = 1024;
            break;
        case BS_2048:
            block_size = 2048;
            break;
        case BS_4096:
            block_size = 4096;
            break;
        default:
            p_set_errno(PCORRUPT);
            close(fd);
            return -1;
    }
    if ((fat_info >> 8) > 32) {
        p_set_errno(PCORRUPT);
        close(fd);
        return -1;
    }

    // Memory map the fat
    size_t fat_size = block_size * (fat_info >> 8);
    uint16_t *fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
    if (fat == MAP_FAILED) {
        p_set_errno(PIOFAIL);
        close(fd);
        return -1;
    }

    // Success
    fs.fd = fd;
    fs.fat = fat;
    fs.block_size = block_size;
    fs.num_blocks = fat_size / 2 - 1;
    if (block_size == 0x2004) {
        // Max-size filesystem, remove 0xffff entry
        fs.num_blocks--;
    }
    if (count_dirents() < 0) {
        munmap(fs.fat, fat_size);
        close(fs.fd);
        fs.fd = -1;
        return -1;
    }
    return 0;
}

int k_umount(void) {
    if (fs.fd < 0) {
        p_set_errno(PNOFS);
        return -1;
    }

    size_t fat_size = (fs.num_blocks + 1) * 2;
    if (fs.num_blocks == 0xfffe) {
        // Max-size filesystem has unused 0xffff FAT entry
        fat_size += 2;
    }

    int result = 0;
    if (munmap(fs.fat, fat_size) < 0) {
        p_set_errno(PIOFAIL);
        close(fs.fd);
        result = -1;
    } else if (close(fs.fd) < 0) {
        p_set_errno(PCLOSEFAIL);
        result = -1;
    }

    fs.fd = -1;
    return 0;
}

uint16_t allocate_block(uint16_t block) {
    for (size_t i = 1; i < fs.num_blocks + 1; i++) {
        if (fs.fat[i] == 0) {
            fs.fat[i] = 0xffff;
            if (block > 0) {
                fs.fat[block] = i;
            }
            return i;
        }
    }
    return 0xffff;
}

/**
 * Do a raw read if "direction" is zero. Do a raw write if it is nonzero.
 */
static int raw_io(uint16_t *block0, uint16_t *block, uint32_t *offset,
                  void *buf, int n, int direction) {
    // Basic validations
    if (n < 0) {
        p_set_errno(PINVAL);
        return -1;
    }
    if (fs.fd < 0) {
        p_set_errno(PNOFS);
        return -1;
    }
    if (n == 0) {
        return 0;
    }

    if (*block == 0xffff && !direction) {
        // Happens when last raw_read stopped short because it ran out of
        // blocks in the file, or if the file doesn't have any blocks allocated
        // yet in the first place
        return 0;
    } else if (*block == 0xffff && *offset == 0) {
        // No blocks in file yet and in write mode. Try to allocate one.
        *block = allocate_block(0);
        if (*block == 0xffff) {
            p_set_errno(PFULL);
            return -1;
        }
        *block0 = *block;
    } else if (*block == 0xffff) {
        // In write mode, not at offset 0, but current block is 0xffff. This
        // only happens if previous raw_writes ran out of space allocating the
        // next block.
        p_set_errno(PFULL);
        return -1;
    }

    if (*block > fs.num_blocks) {
        p_set_errno(PCORRUPT);
        return -1;
    }

    // If offset is right at the start of a block, need to advance to the next
    // block (unless it's the very first block)
    size_t block_offset = *offset % fs.block_size;
    if (block_offset == 0 && *offset != 0) {
        // Reached the end of a block. Get the next one.
        if (fs.fat[*block] == 0xffff && direction) {
            allocate_block(*block);
        }
        *block = fs.fat[*block];
        if (*block == 0xffff) {
            // Last block of file for read or no blocks left for write
            if (direction) {
                p_set_errno(PFULL);
                return -1;
            } else {
                return 0;
            }
        }
        if (*block > fs.num_blocks) {
            // Previous read/write found an invalid block number
            p_set_errno(PCORRUPT);
            return -1;
        }
    }

    // Calculate the raw offset in the file system and go to it
    size_t raw_offset = (fs.num_blocks + 1) * 2;
    if (fs.num_blocks == 0xfffe) {
        // Max-size filesystem has unused 0xffff FAT entry
        raw_offset += 2;
    }
    raw_offset += (*block - 1) * fs.block_size; // First block is 1, not 0
    raw_offset += block_offset;
    if (lseek(fs.fd, raw_offset, SEEK_SET) < 0) {
        p_set_errno(PIOFAIL);
        return -1;
    }

    // Read/write one block at a time
    size_t bytes_processed = 0;
    size_t to_process;
    while (1) {
        if (n > fs.block_size - block_offset) {
            to_process = fs.block_size - block_offset;
        } else {
            to_process = n;
        }
        int result;
        if (direction) {
            result = write(fs.fd, buf, to_process);
        } else {
            result = read(fs.fd, buf, to_process);
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            p_set_errno(PIOFAIL);
            if (bytes_processed > 0) {
                return bytes_processed;
            } else {
                return -1;
            }
        } else if (result == 0) {
            // Hit EOF in the underlying host file in the middle of a block.
            p_set_errno(PCORRUPT);
            if (bytes_processed > 0) {
                return bytes_processed;
            } else {
                return -1;
            }
        }
        bytes_processed += result;
        *offset += result;
        buf += result;
        n -= result;
        if (n == 0) {
            // Don't advance the block if the read/write is done
            break;
        }
        block_offset += result;
        if (block_offset == fs.block_size) {
            // Reached the end of a block. Get the next one.
            if (fs.fat[*block] == 0xffff && direction) {
                allocate_block(*block);
            }
            *block = fs.fat[*block];
            block_offset = 0;
            if (*block == 0xffff) {
                // Last block of file for read or no blocks left for write
                if (bytes_processed > 0 || !direction) {
                    return bytes_processed;
                } else {
                    p_set_errno(PFULL);
                    return -1;
                }
            }
            if (*block > fs.num_blocks) {
                // Next block number is out of range
                p_set_errno(PCORRUPT);
                if (bytes_processed > 0) {
                    return bytes_processed;
                } else {
                    return -1;
                }
            }
            raw_offset = (fs.num_blocks + 1) * 2;
            if (fs.num_blocks == 0xfffe) {
                // Max-size filesystem has unused 0xffff FAT entry
                raw_offset += 2;
            }
            raw_offset += (*block - 1) * fs.block_size; // First is 1, not 0
            if (lseek(fs.fd, raw_offset, SEEK_SET) < 0) {
                p_set_errno(PIOFAIL);
                if (bytes_processed > 0) {
                    return bytes_processed;
                } else {
                    return -1;
                }
            }
        }
    }

    return bytes_processed;
}

int raw_read(uint16_t *block, uint32_t *offset, void *buf, int n) {
    return raw_io(NULL, block, offset, buf, n, 0);
}

int raw_write(uint16_t *block0, uint16_t *block, uint32_t *offset,
              const void *buf, int n) {
    return raw_io(block0, block, offset, (void *) buf, n, 1);
}

uint16_t seek_block(uint16_t block, uint32_t offset) {
    if (fs.fd < 0) {
        p_set_errno(PNOFS);
        return 0;
    }
    if (block == 0xffff) {
        return 0xffff;
    }
    if (block == 0 || block > fs.num_blocks) {
        p_set_errno(PINVAL);
        return 0;
    }
    while (offset > fs.block_size) {
        if (block == 0xffff) {
            // Offset too large. Passed the last block of the file.
            // This means the caller made a mistake.
            p_set_errno(PINTERNAL);
            return 0;
        }
        block = fs.fat[block];
        if (block == 0 || block > fs.num_blocks) {
            p_set_errno(PCORRUPT);
            return 0;
        }
        offset -= fs.block_size;
    }
    return block;
}

int free_blocks(uint16_t block) {
    if (fs.fd < 0) {
        p_set_errno(PNOFS);
        return -1;
    }
    if (block == 0xffff) {
        return 0;
    }
    if (block == 0 || block > fs.num_blocks) {
        p_set_errno(PINVAL);
        return -1;
    }
    while (1) {
        uint16_t old_block = block;
        block = fs.fat[block];
        fs.fat[old_block] = 0;
        if (block == 0xffff) {
            return 0;
        }
        if (block == 0 || block > fs.num_blocks) {
            p_set_errno(PCORRUPT);
            return -1;
        }
    }
}
