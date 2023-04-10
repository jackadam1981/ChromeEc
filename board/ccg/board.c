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
	int up_down;
	char *e;

	up_down = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* booting up */
	if (up_down) {
		ccprintf("booting up\n");
		gpio_set_level(GPIO_EC_DS3, 1);
		msleep(100);
		gpio_set_level(GPIO_DSW_PWROK_EC, 1);
		msleep(100);
		gpio_set_level(GPIO_PM_RSMRST_EC, 1);
		/* shutting down */
	} else {
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
