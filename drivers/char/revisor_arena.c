// SPDX-License-Identifier: GPL-2.0
/*
 * Revisor arena driver — exposes a shared memory region to guest userspace.
 *
 * The arena is a contiguous region of guest physical memory pre-mapped by the
 * hypervisor via KVM_SET_USER_MEMORY_REGION. This driver:
 *
 *  1. Exposes /dev/revisor-arena (misc device).
 *  2. mmap() on the device maps the arena physical pages via remap_pfn_range().
 *  3. REVISOR_CREATE_REGION ioctl returns an anonymous fd whose mmap() maps
 *     a sub-region of the arena (for per-shmem-region fds).
 *  4. REVISOR_DOORBELL ioctl writes to a doorbell MMIO address (guest→host).
 *  5. read()/poll() block until a host→guest IRQ notification arrives.
 *
 * Kernel command line: revisor_arena=<base>,<size>,<irq>,<doorbell>
 * Example: revisor_arena=0x40000000,0x1000000,5,0x3ffff000
 */

#include <linux/anon_inodes.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* ioctl definitions — must match guest/init.c */
struct revisor_region {
	__u64 offset;
	__u64 size;
};

#define REVISOR_CREATE_REGION	_IOW('R', 1, struct revisor_region)
#define REVISOR_DOORBELL	_IO('R', 2)
#define REVISOR_GET_SIZE	_IOR('R', 3, __u64)

/* Parsed from kernel command line. */
static phys_addr_t arena_base;
static size_t arena_size;
static int arena_irq = -1;
static phys_addr_t doorbell_phys;
static void __iomem *doorbell_va;

/* Host→guest notification via IRQ. */
static DECLARE_WAIT_QUEUE_HEAD(arena_waitq);
static atomic_t arena_event_count = ATOMIC_INIT(0);

/* ---- per-region anonymous fd ---- */

struct region_data {
	phys_addr_t phys_base;
	size_t size;
};

static int region_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct region_data *rd = file->private_data;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (vma->vm_pgoff != 0)
		return -EINVAL;
	if (size > rd->size)
		return -EINVAL;

	return remap_pfn_range(vma, vma->vm_start,
			       rd->phys_base >> PAGE_SHIFT,
			       size, vma->vm_page_prot);
}

static int region_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations region_fops = {
	.owner   = THIS_MODULE,
	.mmap    = region_mmap,
	.release = region_release,
};

/* ---- /dev/revisor-arena ---- */

static int arena_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (off + size > arena_size)
		return -EINVAL;

	return remap_pfn_range(vma, vma->vm_start,
			       (arena_base + off) >> PAGE_SHIFT,
			       size, vma->vm_page_prot);
}

static long arena_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case REVISOR_CREATE_REGION: {
		struct revisor_region r;
		struct region_data *rd;
		int fd;

		if (copy_from_user(&r, (void __user *)arg, sizeof(r)))
			return -EFAULT;
		if (r.size == 0)
			return -EINVAL;
		if (r.offset & (PAGE_SIZE - 1))
			return -EINVAL;
		if (r.size & (PAGE_SIZE - 1))
			return -EINVAL;
		if (r.offset + r.size > arena_size)
			return -EINVAL;

		rd = kmalloc(sizeof(*rd), GFP_KERNEL);
		if (!rd)
			return -ENOMEM;

		rd->phys_base = arena_base + r.offset;
		rd->size = r.size;

		fd = anon_inode_getfd("revisor-region", &region_fops, rd,
				      O_RDWR | O_CLOEXEC);
		if (fd < 0)
			kfree(rd);

		return fd;
	}

	case REVISOR_DOORBELL:
		if (doorbell_va)
			writel(1, doorbell_va);
		return 0;

	case REVISOR_GET_SIZE: {
		__u64 sz = arena_size;
		if (copy_to_user((void __user *)arg, &sz, sizeof(sz)))
			return -EFAULT;
		return 0;
	}
	}

	return -ENOTTY;
}

static __poll_t arena_poll(struct file *file, poll_table *wait)
{
	__poll_t mask = 0;

	poll_wait(file, &arena_waitq, wait);

	if (atomic_read(&arena_event_count) > 0)
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static ssize_t arena_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	uint32_t val;

	if (count < sizeof(val))
		return -EINVAL;

	if (file->f_flags & O_NONBLOCK) {
		val = atomic_xchg(&arena_event_count, 0);
		if (val == 0)
			return -EAGAIN;
	} else {
		if (wait_event_interruptible(arena_waitq,
				atomic_read(&arena_event_count) > 0))
			return -ERESTARTSYS;
		val = atomic_xchg(&arena_event_count, 0);
	}

	if (copy_to_user(buf, &val, sizeof(val)))
		return -EFAULT;

	return sizeof(val);
}

static const struct file_operations arena_fops = {
	.owner          = THIS_MODULE,
	.mmap           = arena_mmap,
	.unlocked_ioctl = arena_ioctl,
	.poll           = arena_poll,
	.read           = arena_read,
};

static struct miscdevice arena_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "revisor-arena",
	.fops  = &arena_fops,
};

/* ---- IRQ handler ---- */

static irqreturn_t arena_irq_handler(int irq, void *dev)
{
	atomic_inc(&arena_event_count);
	wake_up_interruptible(&arena_waitq);
	return IRQ_HANDLED;
}

/* ---- init / command line ---- */

static int __init revisor_arena_setup(char *str)
{
	char *p;

	arena_base = simple_strtoul(str, &p, 0);
	if (*p != ',')
		return 0;
	arena_size = simple_strtoul(p + 1, &p, 0);
	if (*p == ',') {
		arena_irq = simple_strtol(p + 1, &p, 0);
		if (*p == ',')
			doorbell_phys = simple_strtoul(p + 1, NULL, 0);
	}

	return 1;
}
__setup("revisor_arena=", revisor_arena_setup);

static int __init revisor_arena_init(void)
{
	int ret;

	if (!arena_base || !arena_size)
		return -ENODEV;

	pr_info("revisor_arena: base=%#llx size=%#zx irq=%d doorbell=%#llx\n",
		(unsigned long long)arena_base, arena_size, arena_irq,
		(unsigned long long)doorbell_phys);

	if (doorbell_phys) {
		doorbell_va = ioremap(doorbell_phys, PAGE_SIZE);
		if (!doorbell_va) {
			pr_err("revisor_arena: ioremap doorbell failed\n");
			return -ENOMEM;
		}
	}

	if (arena_irq >= 0) {
		ret = request_irq(arena_irq, arena_irq_handler,
				  0, "revisor-arena", NULL);
		if (ret) {
			pr_err("revisor_arena: request_irq(%d) failed: %d\n",
			       arena_irq, ret);
			goto err_unmap;
		}
	}

	ret = misc_register(&arena_miscdev);
	if (ret)
		goto err_irq;

	return 0;

err_irq:
	if (arena_irq >= 0)
		free_irq(arena_irq, NULL);
err_unmap:
	if (doorbell_va)
		iounmap(doorbell_va);
	return ret;
}
device_initcall(revisor_arena_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Revisor shared memory arena driver");
