/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "usb_api.h"

#define CC1_VALID 3
#define CC2_VALID 3

static uint16_t debug_connect = (1 << 10) | (1 << 5);

void rdd_interrupt(void)
{
	if (GREAD(RDD, PROG_DEBUG_STATE_MAP) == debug_connect) {
		ccprintf("Debug Accessory connected\n");

		/* Detect when debug cable is disconnected */
		GWRITE(RDD, PROG_DEBUG_STATE_MAP, ~debug_connect);

		/* Select the debug PHY */
		usb_select_phy(USB_SEL_PHY1);

		gpio_set_level(GPIO_CCD_MODE, 1);
	} else {
		ccprintf("Debug Accessory disconnected\n");

		/* Detect when debug cable is connected */
		GWRITE(RDD, PROG_DEBUG_STATE_MAP, debug_connect);

		/* Select the AP PHY */
		usb_select_phy(USB_SEL_PHY0);

		gpio_set_level(GPIO_CCD_MODE, 0);
	}
	/* Connect to selected phy */
	usb_init();

	/* Clear interrupt */
	GWRITE_FIELD(RDD, INT_STATE, INTR_DEBUG_STATE_DETECTED, 1);
}
DECLARE_IRQ(GC_IRQNUM_RDD0_INTR_DEBUG_STATE_DETECTED_INT, rdd_interrupt, 1);

void rdd_init(void)
{
	/* Enable RDD */
	clock_enable_module(MODULE_RDD, 1);
	GWRITE(RDD, POWER_DOWN_B, 1);

	/* Enable RDD interrupts */
	task_enable_irq(GC_IRQNUM_RDD0_INTR_DEBUG_STATE_DETECTED_INT);
	GWRITE_FIELD(RDD, INT_ENABLE, INTR_DEBUG_STATE_DETECTED, 1);

	/* Set valid debug detection values */
	GWRITE(RDD, PROG_DEBUG_STATE_MAP, debug_connect);
}
DECLARE_HOOK(HOOK_INIT, rdd_init, HOOK_PRIO_DEFAULT);

static int command_test_rdd(int argc, char **argv)
{
	GWRITE_FIELD(RDD, INT_TEST, INTR_DEBUG_STATE_DETECTED, 1);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(test_rdd, command_test_rdd, "", "", NULL);
