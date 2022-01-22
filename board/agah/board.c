/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "button.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/retimer/ps8811.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "fw_config.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "throttle_ap.h"
#include "usbc_config.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

static int block_sequence;

__override void board_cbi_init(void)
{
}

enum ec_error_list
board_a1_ps8811_retimer_init(const struct usb_mux *me)
{
	return EC_SUCCESS;
}

static int ps8811_retimer_init(const struct usb_mux *me)
{
	int rv;
	int tries = 2;

	do {
		int val;

		rv = ps8811_i2c_read(me, PS8811_REG_PAGE1,
				     PS8811_REG1_USB_BEQ_LEVEL, &val);
	} while (rv && --tries);

	if (rv) {
		CPRINTS("A1: PS8811 retimer not detected!");
		return rv;
	}
	CPRINTS("A1: PS8811 retimer detected");
	rv = board_a1_ps8811_retimer_init(me);
	if (rv)
		CPRINTS("A1: Error during PS8811 setup rv:%d", rv);
	return rv;
}

const struct usb_mux usba1_ps8811 = {
	.usb_port = USBA_PORT_A1,
	.i2c_port = I2C_PORT_USBA1_RT,
	.i2c_addr_flags = PS8811_I2C_ADDR_FLAGS0,
	.board_init = &ps8811_retimer_init,
};

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	/* Allow keyboard backlight to be enabled */
	gpio_set_level(GPIO_EC_KB_BL_EN, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	/* Turn off the keyboard backlight if it's on. */
	gpio_set_level(GPIO_EC_KB_BL_EN, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

static void board_init(void)
{
	struct usb_mux a1_retimer;

	if ((system_get_reset_flags() & EC_RESET_FLAG_AP_OFF) ||
			(keyboard_scan_get_boot_keys() & BOOT_KEY_DOWN_ARROW)) {
		CPRINTS("PG_PP3300_S5_OD block is enabled");
		block_sequence = 1;
	}
	gpio_enable_interrupt(GPIO_PG_PP3300_S5_OD);
	gpio_enable_interrupt(GPIO_BJ_ADP_PRESENT_ODL);

	a1_retimer = usba1_ps8811;
	a1_retimer.board_init(&a1_retimer);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/**
 * Deferred function to handle GPIO PG_PP3300_S5_OD change
 */
static void bypass_pp3300_s5_deferred(void)
{
	if (block_sequence) {
		CPRINTS("PG_PP3300_S5_OD is blocked.");
		return;
	}

	gpio_set_level(GPIO_PG_PP3300_S5_EC_SEQ_OD,
		       gpio_get_level(GPIO_PG_PP3300_S5_OD));
}
DECLARE_DEFERRED(bypass_pp3300_s5_deferred);

void board_power_interrupt(enum gpio_signal signal)
{
	/* Trigger deferred notification of gpio PG_PP3300_S5_OD change */
	hook_call_deferred(&bypass_pp3300_s5_deferred_data, 0);
}

static int cc_blockseq(int argc, char *argv[])
{
	if (argc > 1) {
		if (!parse_bool(argv[1], &block_sequence)) {
			ccprintf("Invalid argument: %s\n", argv[1]);
			return EC_ERROR_INVAL;
		}
	}

	ccprintf("PG_PP3300_S5_OD block is %s\n",
		 block_sequence ? "enabled" : "disabled");
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(blockseq, cc_blockseq, "[on/off]", NULL);
