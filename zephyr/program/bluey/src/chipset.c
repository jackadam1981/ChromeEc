/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bluey chipset-specific configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "power/qcom.h"

void board_chipset_startup(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_3v_s3_en), 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

void board_chipset_shutdown(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_3v_s3_en), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

void passthru_lid_open_to_pmic(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_pmic_lid_open_od),
			gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_lid_open)));
}

void passthru_ac_on_to_pmic(void)
{
	gpio_pin_set_dt(
		GPIO_DT_FROM_NODELABEL(gpio_ec_pmic_acok),
		gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_acok_od_z5)));
}
