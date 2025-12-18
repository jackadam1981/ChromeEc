/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(stack_protection, LOG_LEVEL_INF);

ZTEST_SUITE(stack_protection, NULL, NULL, NULL, NULL, NULL);

extern uint8_t z_interrupt_stacks[];
static volatile ZTEST_BMEM int fault_reason;

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	zassert_equal(reason, fault_reason);
	ztest_set_fault_valid(false);
}

void trigger_irq_stack_fault(void)
{
	volatile uint8_t *address = (uint8_t *)z_interrupt_stacks;

	LOG_INF("Attempting to write to IRQ stack guard at address: %p\n",
		address);

	*address = 0x00;

	/* Should never reach this. */
	zassert_unreachable();
}

/* Recursive function to consume stack depth */
__attribute__((noinline, optnone)) void
recurse_and_bust_stack(volatile int depth)
{
	/*
	 * Create a purely local array to eat up stack space quickly.
	 * Volatile prevents the compiler from optimizing it away.
	 */
	volatile uint8_t stack_eater[128];

	/* Touch the memory to ensure it's actually allocated/used */
	stack_eater[0] = (uint8_t)depth;

	/* Recursion ensures the Stack Pointer (SP) moves down */
	if (depth >= 0) {
		recurse_and_bust_stack(depth + 1);
	}
}

ZTEST(stack_protection, test_trigger_irq_stack_fault)
{
	LOG_INF("Starting irq stack fault test...\n");

	fault_reason = K_ERR_CPU_EXCEPTION;
	ztest_set_fault_valid(true);

	trigger_irq_stack_fault();

	/* Should never reach this. */
	zassert_unreachable();
}

ZTEST(stack_protection, test_trigger_main_stack_overflow)
{
	LOG_INF("Starting main stack overflow test...\n");

	fault_reason = K_ERR_STACK_CHK_FAIL;
	ztest_set_fault_valid(true);

	/*
	 * This will eventually hit the PMP guard (RISC-V)
	 * OR the MSPLIM register limit (ARM).
	 */
	recurse_and_bust_stack(0);

	/* Should never reach this. */
	zassert_unreachable();
}
