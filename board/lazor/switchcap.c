/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "power/qcom.h"
#include "system.h"
#include "sku.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_I2C, format, ## args)

static int board_use_buck_ic(void)
{
	/* LIMOZEEN model use buck ic */
	return board_is_clamshell();
}

static void switchcap_init(void)
{
	if (board_use_buck_ic()) {
		CPRINTS("Use Buck IC");
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
	if (board_use_buck_ic())
		gpio_set_level(GPIO_VBOB_EN, enable);
	else
		gpio_set_level(GPIO_SWITCHCAP_ON, enable);
}

int board_is_switchcap_enabled(void)
{
	if (board_use_buck_ic())
		return gpio_get_level(GPIO_VBOB_EN);
	else
		return gpio_get_level(GPIO_SWITCHCAP_ON);
}

int board_is_switchcap_power_good(void)
{
	if (board_use_buck_ic())
		/* No way to check POWER GOOD */
		return 1;
	else
		return gpio_get_level(GPIO_DA9313_GPIO0);
}
