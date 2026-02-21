// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI timer initialization
 *
 * Replaces arm64's time.c.  No arch timer hardware; timekeeping
 * will be backed by ksi_time_now() from the host.
 *
 * Follows L4Linux's approach: skip timer_probe() and provide
 * a virtual clocksource/clockevent at 1 MHz.
 */

#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stacktrace.h>
#include <linux/timex.h>

#include <asm/pv_calls.h>
#include <asm/thread_info.h>

/*
 * profile_pc() — same as arm64's version, walks the stack to
 * find the profiling PC (skipping lock functions).
 */
static bool profile_pc_cb(void *arg, unsigned long pc)
{
	unsigned long *prof_pc = arg;

	if (in_lock_functions(pc))
		return true;
	*prof_pc = pc;
	return false;
}

unsigned long profile_pc(struct pt_regs *regs)
{
	unsigned long prof_pc = 0;

	arch_stack_walk(profile_pc_cb, &prof_pc, current, regs);

	return prof_pc;
}
EXPORT_SYMBOL(profile_pc);

/*
 * KSI clocksource — backed by ksi_time_now().
 * For the skeleton, reads return 0 (stub panics).
 * A real implementation returns nanoseconds from the host.
 */
static u64 ksi_clocksource_read(struct clocksource *cs)
{
	return ksi_time_now();
}

static struct clocksource ksi_clocksource = {
	.name	= "ksi",
	.rating	= 400,
	.read	= ksi_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

/*
 * time_init() — called from start_kernel().
 *
 * Register KSI clocksource and calibrate the delay loop.
 * Skip timer_probe() (no arch timer in device tree).
 * Skip arch_timer_get_rate() (no hardware timer).
 */
void __init time_init(void)
{
	u32 ksi_timer_rate = 1000000;	/* 1 MHz virtual timer */

	/*
	 * Register the KSI clocksource.  Mult/shift are set so that
	 * nanoseconds = cycles * mult >> shift.  At 1 MHz:
	 * 1 cycle = 1000 ns, so mult=1000, shift=0.
	 */
	ksi_clocksource.mult = 1000;
	ksi_clocksource.shift = 0;
	clocksource_register_hz(&ksi_clocksource, ksi_timer_rate);

	/* Calibrate the delay loop directly */
	lpj_fine = ksi_timer_rate / HZ;
}
