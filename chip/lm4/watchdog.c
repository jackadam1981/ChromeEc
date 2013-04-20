/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "clock.h"
#include "common.h"
#include "registers.h"
#include "gpio.h"
#include "hooks.h"
#include "panic.h"
#include "task.h"
#include "util.h"
#include "watchdog.h"

/*
 * We use watchdog 0 which is clocked on the system clock
 * to avoid the penalty cycles on each write access
 */

/* magic value to unlock the watchdog registers */
#define LM4_WATCHDOG_MAGIC_WORD  0x1ACCE551

static uint32_t watchdog_period;     /* Watchdog counter initial value */

void IRQ_HANDLER(LM4_IRQ_WATCHDOG)(void) __attribute__((naked));
void IRQ_HANDLER(LM4_IRQ_WATCHDOG)(void)
{
	asm volatile(
		"mov r0, %[pregs]\n"
		"mrs r1, psp\n"
		"mrs r2, ipsr\n"
		"mov r3, sp\n"
		"stmia r0, {r1-r11, lr}\n"
		"mov sp, %[pstack]\n"           /* switch to panic stack */
		"mov r0, lr\n"
		"push {r0, r3}\n"
		"bl watchdog_trace\n"
		"mov r0, %[irq]\n"
		"bl task_disable_irq\n"
		"pop {r0, r3}\n"
		"mov lr, r0\n"
		"mov sp, r3\n"                  /* restore exception stack */
		"b task_resched_if_needed\n" : :
			[pregs] "r" (pdata_ptr->regs),
			[pstack] "r" (pstack_addr),
			[irq] "i" (LM4_IRQ_WATCHDOG) :
			/* Constraints protecting regs from being clobbered.
			 * Gcc will use r0 & r12 for pregs & pstack. */
			"r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
			"r10", "r11", "cc", "memory"
	);
}
const struct irq_priority IRQ_BUILD_NAME(prio_, LM4_IRQ_WATCHDOG, )
	__attribute__((section(".rodata.irqprio")))
		= {LM4_IRQ_WATCHDOG, 0}; /* put the watchdog at the highest
					    priority */

void watchdog_reload(void)
{
	uint32_t status = LM4_WATCHDOG_RIS(0);

	/* Unlock watchdog registers */
	LM4_WATCHDOG_LOCK(0) = LM4_WATCHDOG_MAGIC_WORD;

	/* As we reboot only on the second timeout, if we have already reached
	 * the first timeout we need to reset the interrupt bit. */
	if (status) {
		LM4_WATCHDOG_ICR(0) = status;
		/* That doesn't seem to unpend the watchdog interrupt (even if
		 * we do dummy writes to force the write to be committed), so
		 * explicitly unpend the interrupt before re-enabling it. */
		task_clear_pending_irq(LM4_IRQ_WATCHDOG);
		task_enable_irq(LM4_IRQ_WATCHDOG);
	}

	/* Reload the watchdog counter */
	LM4_WATCHDOG_LOAD(0) = watchdog_period;

	/* Re-lock watchdog registers */
	LM4_WATCHDOG_LOCK(0) = 0xdeaddead;
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

static void watchdog_freq_changed(void)
{
	/* Set the timeout period */
	watchdog_period = WATCHDOG_PERIOD_MS * (clock_get_freq() / 1000);

	/* Reload the watchdog timer now */
	watchdog_reload();
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, watchdog_freq_changed, HOOK_PRIO_DEFAULT);

int watchdog_init(void)
{
	volatile uint32_t scratch  __attribute__((unused));

	/* Enable watchdog 0 clock */
	LM4_SYSTEM_RCGCWD |= 0x1;
	/* Wait 3 clock cycles before using the module */
	scratch = LM4_SYSTEM_RCGCWD;

	/* Set initial timeout period */
	watchdog_freq_changed();

	/* Unlock watchdog registers */
	LM4_WATCHDOG_LOCK(0) = LM4_WATCHDOG_MAGIC_WORD;

	/* De-activate the watchdog when the JTAG stops the CPU */
	LM4_WATCHDOG_TEST(0) |= 1 << 8;

	/* Reset after 2 time-out, activate the watchdog and lock the control
	 * register. */
	LM4_WATCHDOG_CTL(0) = 0x3;

	/* Reset watchdog interrupt bits */
	LM4_WATCHDOG_ICR(0) = LM4_WATCHDOG_RIS(0);

	/* Lock watchdog registers against unintended accesses */
	LM4_WATCHDOG_LOCK(0) = 0xdeaddead;

	/* Enable watchdog interrupt */
	task_enable_irq(LM4_IRQ_WATCHDOG);

	return EC_SUCCESS;
}
