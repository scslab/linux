// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor file I/O helpers
 *
 * These functions provide file I/O capabilities for the Rust kVisor
 * implementation, since the kernel Rust bindings don't expose filp_open
 * and kernel_read directly.
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/stat.h>

#include "file_helper.h"

/*
 * kvisor_file_open - Open a file by path
 * @path: Kernel-space NUL-terminated path string
 * @flags: Open flags (O_RDONLY, etc.)
 *
 * Returns a file pointer on success, ERR_PTR on failure.
 */
struct file *kvisor_file_open(const char *path, int flags)
{
	return filp_open(path, flags, 0);
}
EXPORT_SYMBOL_GPL(kvisor_file_open);

/*
 * kvisor_file_close - Close a file
 * @filp: File pointer to close
 */
void kvisor_file_close(struct file *filp)
{
	if (filp && !IS_ERR(filp))
		filp_close(filp, NULL);
}
EXPORT_SYMBOL_GPL(kvisor_file_close);

/*
 * kvisor_file_size - Get the size of a file
 * @filp: File pointer
 *
 * Returns the file size in bytes, or negative error code.
 */
long long kvisor_file_size(struct file *filp)
{
	struct kstat stat;
	int ret;

	if (!filp || IS_ERR(filp))
		return -EINVAL;

	ret = vfs_getattr(&filp->f_path, &stat, STATX_SIZE, AT_STATX_SYNC_AS_STAT);
	if (ret)
		return ret;

	return stat.size;
}
EXPORT_SYMBOL_GPL(kvisor_file_size);

/*
 * kvisor_file_read - Read from a file into a kernel buffer
 * @filp: File pointer
 * @buf: Kernel buffer to read into
 * @count: Number of bytes to read
 * @pos: File position to read from
 *
 * Returns number of bytes read, or negative error code.
 */
ssize_t kvisor_file_read(struct file *filp, void *buf, size_t count, loff_t *pos)
{
	if (!filp || IS_ERR(filp))
		return -EINVAL;

	return kernel_read(filp, buf, count, pos);
}
EXPORT_SYMBOL_GPL(kvisor_file_read);

/*
 * kvisor_file_read_alloc - Read entire file into newly allocated buffer
 * @path: Kernel-space NUL-terminated path string
 * @buf_out: Output pointer to allocated buffer
 * @size_out: Output pointer to file size
 *
 * Allocates a buffer with kvmalloc and reads the entire file into it.
 * Caller must free the buffer with kvfree().
 *
 * Returns 0 on success, negative error code on failure.
 */
int kvisor_file_read_alloc(const char *path, void **buf_out, size_t *size_out)
{
	struct file *filp;
	long long size;
	void *buf;
	loff_t pos = 0;
	ssize_t ret;

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp))
		return PTR_ERR(filp);

	size = kvisor_file_size(filp);
	if (size < 0) {
		filp_close(filp, NULL);
		return size;
	}

	/* Sanity check: don't load files larger than 256MB */
	if (size > (256 * 1024 * 1024)) {
		filp_close(filp, NULL);
		return -EFBIG;
	}

	buf = kvmalloc(size, GFP_KERNEL);
	if (!buf) {
		filp_close(filp, NULL);
		return -ENOMEM;
	}

	ret = kernel_read(filp, buf, size, &pos);
	filp_close(filp, NULL);

	if (ret < 0) {
		kvfree(buf);
		return ret;
	}

	if (ret != size) {
		kvfree(buf);
		return -EIO;
	}

	*buf_out = buf;
	*size_out = size;
	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_file_read_alloc);

/*
 * kvisor_file_free - Free a buffer allocated by kvisor_file_read_alloc
 * @buf: Buffer to free
 */
void kvisor_file_free(void *buf)
{
	if (buf)
		kvfree(buf);
}
EXPORT_SYMBOL_GPL(kvisor_file_free);
