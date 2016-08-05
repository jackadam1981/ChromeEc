/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_RBOX, format, ## args)

/* Falling edge of power button */
static void power_button_poked(void)
{
	CPRINTS("power_button");

	/* clear interrupt bit */
	GWRITE_FIELD(RBOX, INT_STATE, INTR_PWRB_IN_FED, 1);
}
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_PWRB_IN_FED_INT, power_button_poked, 1);

void rbox_init(void)
{
	uint32_t wakeup_intr;

	/* Enable RBOX */
	clock_enable_module(MODULE_RBOX, 1);

	wakeup_intr = GREG32(RBOX, WAKEUP_INTR);
	CPRINTS("RBOX wakeup 0x%x", wakeup_intr);

	/* Clear the wakeup status bits */
	GREG32(RBOX, WAKEUP) = GC_RBOX_WAKEUP_CLEAR_MASK;
	/* Enable wake-on-rbox for next time */
	GREG32(RBOX, WAKEUP) = GC_RBOX_WAKEUP_ENABLE_MASK;

	/* Enable power button interrupt */
	GWRITE_FIELD(RBOX, INT_ENABLE, INTR_PWRB_IN_FED, 1);
	task_enable_irq(GC_IRQNUM_RBOX0_INTR_PWRB_IN_FED_INT);
}
DECLARE_HOOK(HOOK_INIT, rbox_init, HOOK_PRIO_DEFAULT - 1);
