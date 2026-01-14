/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kVisor file I/O helpers
 */

#ifndef _KVISOR_FILE_HELPER_H
#define _KVISOR_FILE_HELPER_H

#include <linux/types.h>

struct file;

/*
 * Open a file by path (kernel-space path).
 * Returns file pointer or ERR_PTR on failure.
 */
struct file *kvisor_file_open(const char *path, int flags);

/*
 * Close a file.
 */
void kvisor_file_close(struct file *filp);

/*
 * Get the size of a file.
 * Returns size in bytes or negative error code.
 */
long long kvisor_file_size(struct file *filp);

/*
 * Read from a file into a kernel buffer.
 * Returns bytes read or negative error code.
 */
ssize_t kvisor_file_read(struct file *filp, void *buf, size_t count, loff_t *pos);

/*
 * Read entire file into newly allocated buffer.
 * Caller must free with kvisor_file_free().
 * Returns 0 on success, negative error code on failure.
 */
int kvisor_file_read_alloc(const char *path, void **buf_out, size_t *size_out);

/*
 * Free a buffer allocated by kvisor_file_read_alloc.
 */
void kvisor_file_free(void *buf);

#endif /* _KVISOR_FILE_HELPER_H */
