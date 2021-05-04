/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "driver/ln9310.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power/sc7180.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

/* LN9310 switchcap */
const struct ln9310_config_t ln9310_config = {
	.i2c_port = I2C_PORT_POWER,
	.i2c_addr_flags = LN9310_I2C_ADDR_0_FLAGS,
};

static int board_has_ln9310(void)
{
	static int ln9310_present = -1;
	int status, val;

	/* Cache the status of LN9310 present or not */
	if (ln9310_present == -1) {
		status = i2c_read8(ln9310_config.i2c_port,
				   ln9310_config.i2c_addr_flags,
				   LN9310_REG_CHIP_ID,
				   &val);

		/*
		 * Any error reading LN9310 CHIP_ID over I2C means the chip
		 * not present. Fallback to use DA9313 switchcap.
		 */
		ln9310_present = !status && val == LN9310_CHIP_ID;
	}

	return ln9310_present;
}

static void switchcap_init(void)
{
	if (board_has_ln9310()) {
		CPRINTS("Use switchcap: LN9310");
	} else {
		CPRINTS("Use switchcap: DA9313");

		/*
		 * When the chip in power down mode, it outputs high-Z.
		 * Set pull-down to avoid floating.
		 */
		gpio_set_flags(GPIO_DA9313_GPIO0, GPIO_INPUT | GPIO_PULL_DOWN);

		/*
		 * Configure DA9313 enable, push-pull output. Don't set the
		 * level here; otherwise, it will override its value and
		 * shutdown the switchcap when sysjump to RW.
		 */
		gpio_set_flags(GPIO_SWITCHCAP_ON, GPIO_OUTPUT);
	}
}
DECLARE_HOOK(HOOK_INIT, switchcap_init, HOOK_PRIO_DEFAULT);

void board_set_switchcap_power(int enable)
{
	gpio_set_level(GPIO_VBOB_EN, enable);
}

int board_is_switchcap_enabled(void)
{
	return gpio_get_level(GPIO_VBOB_EN);
}

int board_is_switchcap_power_good(void)
{
	/* No way to check POWER GOOD */
	return 1;
}
