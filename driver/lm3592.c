/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI lm3592 driver.
 */

#include "console.h"
#include "i2c.h"
#include "lm3592.h"
#include "timer.h"
#include "util.h"

inline int lm3592_write(uint8_t reg, uint8_t val)
{
	return i2c_write8(I2C_PORT_KBBL, LM3592_I2C_ADDR, reg, val);
}

inline int lm3592_read(uint8_t reg, int *val)
{
	return i2c_read8(I2C_PORT_KBBL, LM3592_I2C_ADDR, reg, val);
}

int lm3592_enable_backlight_power(int enabled)
{
	int ret = 0;

	if (enabled) {
		ret |= lm3592_write(LM3592_REG_GP, 0x7);
		ret |= lm3592_write(LM3592_REG_BMAIN, 0x1F);
	} else {
		ret |= lm3592_write(LM3592_REG_GP, 0x5);
		ret |= lm3592_write(LM3592_REG_BMAIN, 0x00);
	}
	if (ret != 0)
		return ret;
	return EC_SUCCESS;

}

/*****************************************************************************/
/* Console commands */
#ifdef CONFIG_CMD_KBBACKLIGHT
static int command_lm3592(int argc, char **argv)
{
	if (argc == 2) {
		int v;

		if (!parse_bool(argv[1], &v))
			return EC_ERROR_PARAM1;

		if (v)
			return lm3592_poweron();
		else
			return lm3592_poweroff();
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(lm3592, command_lm3592,
			"on | off", NULL);
#endif
