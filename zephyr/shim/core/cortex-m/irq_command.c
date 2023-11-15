/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "task.h"

#include <cmsis_core.h>

/* IRQ counters */
int irq_count[CONFIG_NUM_IRQS];
int last_irq;

void sys_trace_isr_enter_user(void)
{
	/* read the exception number */
	uint32_t irq = __get_IPSR() - 16;

	__ASSERT(irq < CONFIG_NUM_IRQS, "Invalid IRQ number");

	last_irq = irq;
	irq_count[irq]++;
}

task_id_t task_get_last_irq(void)
{
	return last_irq;
}

uint32_t task_get_last_irq_count(void)
{
	__ASSERT(last_irq < CONFIG_NUM_IRQS, "Invalid IRQ number");
	return irq_count[last_irq];
}

static int command_irq(int argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for (int i = 0; i < CONFIG_NUM_IRQS; i++) {
		if (irq_count[i])
			ccprintf("  IRQ %d: %d\n", i, irq_count[i]);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(irq, command_irq, "", "List irq counters");
