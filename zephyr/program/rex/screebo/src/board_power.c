/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_pwrseq_sm.h"
#include "timer.h"

#include <power_signals.h>

static int board_ap_power_action_s0_run(void *data)
{
	if (power_signal_get(PWR_PCH_PWROK) &&
	    !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en))) {
		usleep(30);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_a1_oc_pu_en),
				1);
	}

	return 0;
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_S0, NULL, board_ap_power_action_s0_run,
			  NULL);
