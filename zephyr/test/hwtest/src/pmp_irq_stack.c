/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(pmp_irq_stack, LOG_LEVEL_INF);

ZTEST_SUITE(pmp_irq_stack, NULL, NULL, NULL, NULL, NULL);

extern uint8_t z_interrupt_stacks[];

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	zassert_equal(reason, K_ERR_CPU_EXCEPTION);
	ztest_set_fault_valid(false);
}

void trigger_irq_stack_fault(void)
{
	volatile uint8_t *address = (uint8_t *)z_interrupt_stacks;

	LOG_INF("Attempting to write to IRQ stack guard at address: %p\n",
		address);

	ztest_set_fault_valid(true);
	*address = 0x00;

	/* Should never reach this. */
	zassert_unreachable();
}

ZTEST(pmp_irq_stack, test_trigger_irq_stack_fault)
{
	trigger_irq_stack_fault();

	/* Should never reach this. */
	zassert_unreachable();
}
