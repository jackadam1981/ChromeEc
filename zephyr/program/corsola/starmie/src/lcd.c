/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Krabby PPC/BC12 (RT1739) configuration */

#include "baseboard_usbc_config.h"
#include "charge_state.h"
#include "console.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ##args)

void ac_feedback_lcd(void)
{
	if (extpower_is_present()) {
		CPRINTS("ac feedback lcd 1");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ac_lcd), 1);
	} else {
		CPRINTS("ac feedback lcd 0");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ac_lcd), 0);
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, ac_feedback_lcd, HOOK_PRIO_DEFAULT);
