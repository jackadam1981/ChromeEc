/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "tablet_mode.h"
#include "usb_pd.h"

#define RESUME  0

/* Wake up pins */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_ACOK_OD,
	GPIO_GSC_EC_PWR_BTN_ODL,
	GPIO_LID_OPEN,
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* This callback disables keyboard when convertibles are fully open */
__override void lid_angle_peripheral_enable(int enable)
{
	/*
	 * If the lid is in tablet position via other sensors,
	 * ignore the lid angle, which might be faulty then
	 * disable keyboard.
	 */
	if (tablet_get_mode())
		enable = 0;

	if (enable) {
		keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_ANGLE);
	} else {
		/*
		 * When the chipset is on, the EC keeps the keyboard enabled and
		 * Ensure that the chipset is off before disabling the keyboard.
		 * the AP decides whether to ignore input devices or not.
		 */
		if (!chipset_in_state(CHIPSET_STATE_ON))
			keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_ANGLE);
		}
}

#ifdef CONFIG_USB_PD_USB4
/*
 * When TBT update procress begin, PD enter suspend.
 * If we press shutdown button when PD suspend, the PD function
 * will be lost during S5.
 * So resume PD from suspend when enter S5.
 */
static void resume_port(void)
{
	int port;

	for  (port = 0; port < CONFIG_USB_PD_PORT_MAX_COUNT; port++) {
		if (!pd_is_port_enabled(port))
			pd_set_suspend(port, RESUME);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, resume_port, HOOK_PRIO_DEFAULT);

#endif /* CONFIG_USB_PD_USB4 */