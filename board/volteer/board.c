/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Volteer board-specific configuration */

#include "common.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

#include "gpio_list.h" /* Must come after other header files. */

static int command_volteer(int argc, char **argv)
{
	int ms = 100;  /* Press duration in ms */
	char *e;

	if (argc > 1) {
		ms = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
	}

	if (gpio_get_level(GPIO_EN_PP3300_A)) {
		ccprintf("PP3300_A rail is already enabled\n");
		return EC_ERROR_BUSY;
	}

	gpio_set_level_verbose(CC_COMMAND, GPIO_EN_PP3300_A, 1);

	msleep(100);

	gpio_set_level_verbose(CC_COMMAND, GPIO_EN_PP5000_A, 1);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(volteer, command_volteer,
			"[delay"],
			"Bring up PP3300 and PP5000 manually");

static void board_init(void)
{
	/* TODO */
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

