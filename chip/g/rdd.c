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

#define CCD_PHY USB_SEL_PHY1

static void change_debug_state(void)
{
	GWRITE(RDD, PROG_DEBUG_STATE_MAP, ~GREAD(RDD, PROG_DEBUG_STATE_MAP));
}

void rdd_interrupt(void)
{
	int ccd_enabled = usb_get_phy() == CCD_PHY;

	ccprintf("Debug Accessory %sconnected\n", ccd_enabled ? "dis" : "");
	/* Detect when debug cable state changes */
	change_debug_state();

	/* Switch the PHY */
	usb_select_phy(ccd_enabled ? USB_SEL_PHY0 : CCD_PHY);

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

	/* Detect cable disconnect if CCD is enabled */
	if (usb_get_phy() == CCD_PHY)
		change_debug_state();
}
DECLARE_HOOK(HOOK_INIT, rdd_init, HOOK_PRIO_DEFAULT);

static int command_test_rdd(int argc, char **argv)
{
	GWRITE_FIELD(RDD, INT_TEST, INTR_DEBUG_STATE_DETECTED, 1);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(test_rdd, command_test_rdd, "", "", NULL);
