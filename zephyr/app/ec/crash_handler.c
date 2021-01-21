/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <arch/cpu.h>
#include <fatal.h>
#include <logging/log.h>
#include <logging/log_ctrl.h>
#include <sys/printk.h>
#include <zephyr.h>

static void arch_print_esf(const z_arch_esf_t *esf)
{
#ifdef CONFIG_ARM
	printk("a1=%08X\n", esf->basic.a1);
	printk("a2=%08X\n", esf->basic.a2);
	printk("a3=%08X\n", esf->basic.a3);
	printk("a4=%08X\n", esf->basic.a4);
	printk("ip=%08X\n", esf->basic.ip);
	printk("lr=%08X\n", esf->basic.lr);
	printk("pc=%08X\n", esf->basic.pc);
	printk("xpsr=%08X\n", esf->basic.xpsr);
#else
	/* Default if not implemented for architecture */
	ARG_UNUSED(esf);
#endif
}

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
	printk("Fatal error: %u\n", reason);

	if (esf)
		arch_print_esf(esf);

	LOG_PANIC();
	arch_system_halt(reason);
	CODE_UNREACHABLE;
}
