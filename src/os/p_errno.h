/** @file
 * User-level API for checking error conditions and printing corresponding
 * messages
 */
#ifndef P_ERRNO_H
#define P_ERRNO_H

/**
 * Returns the error code set when a function fails for the
 * currently scheduled process.
 *
 * @return the error code.
 */
int p_errno();

/**
 * Sets the error number of the current process.
 *
 * @param p_errno   The error number to set
 */
void p_set_errno(int p_errno);

/**
 * Print an error message corresponding to the current value of p_errno,
 * followed by a newline
 *
 * @param msg If provided, this message will be printed with a colon before the
 *            error message.
 */
void p_perror(const char *msg);

/**
 * Possible values for p_errno
 */
enum {
    PINVAL = 1, /**< Invalid parameter(s) */
    PNOMEM,     /**< Could not allocate memory */
    PPERM,      /**< Proper permissions not granted */
    POPENFAIL,  /**< Cannot open file from host */
    PIOFAIL,    /**< Cannot read/write file from host */
    PCLOSEFAIL, /**< Cannot close file from host */
    PMOUNTED,   /**< Tried to mount when a filesystem is already mounted */
    PCORRUPT,   /**< Filesystem is corrupt */
    PINUSE,     /**< Tried to umount when files still open */
    PWLOCK,     /**< Tried to open a file for writing that was already open */
    PNOFS,      /**< Tried to access filesystem when not mounted */
    PNOFILE,    /**< File does not exist */
    PFULL,      /**< Filesystem has no more free blocks */
    PMODE,      /**< File not open in the proper mode */
    PTOOFAR,    /**< Trying to seek outside the file */
    PINTERNAL,  /**< Internal error. There is a bug. */
    KECHILD,    /**< No child process to wait on. */
    KBADFD,     /**< Invalid file descriptor. */
    KNOFD,      /**< No more file descriptors left. */
    KBADPRIO,   /**< Invalid process priority */
    KBADPID,    /**< A process with the specified PID was not found. */
    KBADSIG,    /**< An invalid signal was specified to a system call. */
    KBADSCHED,  /**< An unschedulable process was attempted to be scheduled. */
};

#endif
