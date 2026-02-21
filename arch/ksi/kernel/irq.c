// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI IRQ initialization
 *
 * Replaces arm64's irq.c.  No physical interrupt controller (GIC);
 * interrupts are delivered via ksi_irq_deliver() from the host.
 *
 * Follows L4Linux's approach: provide a minimal init_IRQ() that
 * skips irqchip probing and hardware-specific setup.
 */

#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/scs.h>

#include <asm/exception.h>
#include <asm/softirq_stack.h>
#include <asm/vmap_stack.h>

/* Per-CPU IRQ context — referenced by arm64 entry.S */
DEFINE_PER_CPU(struct nmi_ctx, nmi_contexts);

DEFINE_PER_CPU(unsigned long *, irq_stack_ptr);

DECLARE_PER_CPU(unsigned long *, irq_shadow_call_stack_ptr);

#ifdef CONFIG_SHADOW_CALL_STACK
DEFINE_PER_CPU(unsigned long *, irq_shadow_call_stack_ptr);
#endif

/*
 * Default IRQ/FIQ handlers — panic if an interrupt arrives
 * before a real handler is installed.
 */
static void default_handle_irq(struct pt_regs *regs)
{
	panic("IRQ taken without a root IRQ handler\n");
}

static void default_handle_fiq(struct pt_regs *regs)
{
	panic("FIQ taken without a root FIQ handler\n");
}

void (*handle_arch_irq)(struct pt_regs *) __ro_after_init = default_handle_irq;
void (*handle_arch_fiq)(struct pt_regs *) __ro_after_init = default_handle_fiq;

int __init set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	if (handle_arch_irq != default_handle_irq)
		return -EBUSY;

	handle_arch_irq = handle_irq;
	pr_info("Root IRQ handler: %ps\n", handle_irq);
	return 0;
}

int __init set_handle_fiq(void (*handle_fiq)(struct pt_regs *))
{
	if (handle_arch_fiq != default_handle_fiq)
		return -EBUSY;

	handle_arch_fiq = handle_fiq;
	pr_info("Root FIQ handler: %ps\n", handle_fiq);
	return 0;
}

#ifndef CONFIG_PREEMPT_RT
static void ____do_softirq(struct pt_regs *regs)
{
	__do_softirq();
}

void do_softirq_own_stack(void)
{
	/*
	 * KSI: run softirqs on the current stack.
	 * The arm64 version uses call_on_irq_stack() which requires
	 * irq_stack_ptr to be set up.  For KSI, we run directly.
	 */
	____do_softirq(NULL);
}
#endif

/*
 * KSI init_IRQ() — minimal initialization.
 *
 * Skip irqchip_init() (no GIC, no device tree irqchip nodes).
 * Skip IRQ stack allocation (requires vmalloc, which needs the
 * linear map to be functional — future work for KSI).
 *
 * The host delivers interrupts via ksi_irq_deliver().
 * A proper KSI irqchip driver will be added later.
 */
void __init init_IRQ(void)
{
	/* IRQ stacks and irqchip will be set up when KSI gets a host */
}
