/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Herobrine chipset-specific configuration */

#include "common.h"
#include "battery.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

/* Delay to allow the power source to transition completely */
#define EN_PP5000_WAIT		PD_T_SRC_TURN_ON

static timestamp_t guard_5v_timeout;

/* Called on USB PD connected */
static void board_usb_pd_connect(void)
{
	int soc = -1;

	if (battery_state_of_charge_abs(&soc) != EC_SUCCESS ||
	    soc < CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON) {
		guard_5v_timeout = get_time();
		guard_5v_timeout.val += EN_PP5000_WAIT;
	}
}
DECLARE_HOOK(HOOK_USB_PD_CONNECT, board_usb_pd_connect, HOOK_PRIO_DEFAULT);

/* Called on AP S5 -> S3 transition */
static void board_chipset_pre_init(void)
{
	static bool pp5000_inited;

	if (!pp5000_inited) {
		if (guard_5v_timeout.val) {
			CPRINTS("Wait PD negotiated VBUS transition %u",
				guard_5v_timeout.le.lo);
			timer_arm(guard_5v_timeout, TASK_ID_CHIPSET);
		}

		CPRINTS("Enable 5V rail");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), 1);
		pp5000_inited = true;
	}
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);
