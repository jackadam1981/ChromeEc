/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/gpio.h>

#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ln9310.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "i2c.h"
#include "power/qcom.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* LN9310 switchcap */
const struct ln9310_config_t ln9310_config = {
	.i2c_port = I2C_PORT_POWER,
	.i2c_addr_flags = LN9310_I2C_ADDR_0_FLAGS,
};

enum battery_cell_type board_get_battery_cell_type(void)
{
	return BATTERY_CELL_TYPE_2S;
}

static void switchcap_init(void)
{
	
	CPRINTS("Use switchcap: LN9310");

	/* Enable interrupt for LN9310 */
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_switchcap_pg));

	/* Only configure the switchcap if not sysjump */
	if (!system_jumped_late()) {
		ln9310_init();
	}
}
DECLARE_HOOK(HOOK_INIT, switchcap_init, HOOK_PRIO_DEFAULT);

void board_set_switchcap_power(int enable)
{
	gpio_pin_set_dt(
		GPIO_DT_FROM_NODELABEL(gpio_switchcap_on),
		enable);
	ln9310_software_enable(enable);
}

int board_is_switchcap_enabled(void)
{
	return gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_switchcap_on));
}

int board_is_switchcap_power_good(void)
{
	return ln9310_power_good();
}