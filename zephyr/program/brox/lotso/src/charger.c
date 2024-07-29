/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "driver/charger/bq257x0_regs.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"

#include <zephyr/drivers/gpio.h>

#define LOTSO_CHARGER_MIN_INPUT_VOLTAGE 0x240
static void bq25710_min_input_voltage(void)
{
	if (extpower_is_present()) {
		i2c_write16(chg_chips[0].i2c_port, chg_chips[0].i2c_addr_flags,
			    BQ25710_REG_INPUT_VOLTAGE,
			    LOTSO_CHARGER_MIN_INPUT_VOLTAGE);
	}
}
DECLARE_HOOK(HOOK_INIT, bq25710_min_input_voltage, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_AC_CHANGE, bq25710_min_input_voltage, HOOK_PRIO_DEFAULT);

static void control_prochot_when_AC_only_deferred(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_prochot_odl), 1);
}
DECLARE_DEFERRED(control_prochot_when_AC_only_deferred);

static void control_prochot(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_prochot_odl), 0);
	hook_call_deferred(&control_prochot_when_AC_only_deferred_data,
			   5 * MINUTE);
}
DECLARE_HOOK(HOOK_INIT, control_prochot, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, control_prochot, HOOK_PRIO_DEFAULT);
