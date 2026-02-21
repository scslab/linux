// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI early boot console
 *
 * Provides console output from the earliest stages of boot by routing
 * printk to ksi_console_write().  Registered before start_kernel() so
 * that even the first pr_notice(linux_banner) is captured.
 *
 * This is a boot console (CON_BOOT) — it will be automatically
 * unregistered when a real console driver takes over.
 */

#include <linux/console.h>
#include <linux/init.h>
#include <asm/pv_calls.h>

static void ksi_early_console_write(struct console *con, const char *s,
				    unsigned int count)
{
	ksi_console_write(s, count);
}

static struct console ksi_early_console = {
	.name	= "ksi",
	.write	= ksi_early_console_write,
	.flags	= CON_PRINTBUFFER | CON_BOOT | CON_ANYTIME,
	.index	= -1,
};

/*
 * Called from ksi_start_kernel() before start_kernel().
 * register_console() is safe here — the console_mutex is statically
 * initialized and there is no concurrency at this point.
 */
void __init ksi_setup_early_console(void)
{
	register_console(&ksi_early_console);
}
