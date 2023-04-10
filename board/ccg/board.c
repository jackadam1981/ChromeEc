/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADLRVP-ITE board-specific configuration */

#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "intc.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

static int console_command_boot(int argc, const char **argv)
{
	char *e;
	int up_down, i;
	const uint32_t pins[][2] = {
		{ GPIO_PM_PWRBTN_N_EC, GPIO_OUT_HIGH },
		{ GPIO_SYS_RST_ODL_EC, GPIO_OUT_HIGH },
		{ GPIO_PCH_WAKE_N, GPIO_ODR_HIGH },
		{ GPIO_EC_DS3, GPIO_OUT_LOW },
		{ GPIO_DSW_PWROK_EC, GPIO_OUT_LOW },
		{ GPIO_PM_RSMRST_EC, GPIO_OUT_LOW },
	};

	up_down = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* Change GPIO direction */
	for (i = 0; i < ARRAY_SIZE(pins); i++)
		gpio_set_flags(pins[i][0], pins[i][1]);

	if (up_down) {
		/* booting up */
		ccprintf("booting up\n");
		gpio_set_level(GPIO_EC_DS3, 1);
		msleep(100);
		gpio_set_level(GPIO_DSW_PWROK_EC, 1);
		msleep(100);
		gpio_set_level(GPIO_PM_RSMRST_EC, 1);
	} else {
		/* shutting down */
		ccprintf("shutting down\n");
		gpio_set_level(GPIO_PM_RSMRST_EC, 0);
		msleep(100);
		gpio_set_level(GPIO_DSW_PWROK_EC, 0);
		msleep(100);
		gpio_set_level(GPIO_EC_DS3, 0);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(boot, console_command_boot, "1 / 0",
			"Command to boot RVP");

#include "gpio_list.h"
/******************************************************************************/
