/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog common code */

#include "common.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

void watchdog_trace(struct panic_data *pdata)
{
	report_exception(pdata);
	/* If we are blocked in a high priority IT handler, the following debug
	 * messages might not appear but they are useless in that situation. */
	timer_print_info();
	task_print_list();
}

void watchdog_exception_handler(void)
{
	asm volatile(
		"mov r3, sp\n"              /* save stack frame */
		"mov r0, sp\n"
		"sub r0, %[pdata_size]\n"   /* allocate panic_data on stack */
		"bic r0, r0, #7\n"          /* 64-bit alignment for ARM EABI */
		"mov sp, r0\n"
		"add r0, %[pregs_offset]\n" /* seek to regs[0] of panic_data */
		"mrs r1, psp\n"
		"mrs r2, ipsr\n"
		"stmia r0, {r1-r11, lr}\n"
		"mov r0, sp\n"              /* arg0 for watchdog_check */
		"push {r3, lr}\n"
		"bl watchdog_check\n"       /* jump to chip specific code */
		"pop {r0, lr}\n"            /* restore EXC_RETURN */
		"mov sp, r0\n"              /* restore stack frame */
		"b task_resched_if_needed\n" : :
			[pdata_size] "i" (sizeof(struct panic_data)),
			[pregs_offset] "i" (__builtin_offsetof(
						struct panic_data, regs))
	);
}
