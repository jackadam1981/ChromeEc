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
		{ GPIO_PM_PWRBTN, GPIO_OUT_HIGH },
		{ GPIO_SYS_RST, GPIO_OUT_HIGH },
		{ GPIO_PCH_WAKE, GPIO_ODR_HIGH },
		{ GPIO_EN_PP3300_A, GPIO_OUT_LOW },
		{ GPIO_DSW_PWROK, GPIO_OUT_LOW },
		{ GPIO_PM_RSMRST, GPIO_OUT_LOW },
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
		gpio_set_level(GPIO_EN_PP3300_A, 1);
		msleep(100);
		gpio_set_level(GPIO_DSW_PWROK, 1);
		msleep(100);
		gpio_set_level(GPIO_PM_RSMRST, 1);
	} else {
		/* shutting down */
		ccprintf("shutting down\n");
		gpio_set_level(GPIO_PM_RSMRST, 0);
		msleep(100);
		gpio_set_level(GPIO_DSW_PWROK, 0);
		msleep(100);
		gpio_set_level(GPIO_EN_PP3300_A, 0);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(boot, console_command_boot, "1 / 0",
			"Command to boot RVP");

#include "gpio_list.h"
/******************************************************************************/
