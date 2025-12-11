/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Bluey chipset-specific configuration */

#include "battery.h"
#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "power/qcom.h"
#include "registers.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

/* Get NPCX9 GLUE registers */
#define NPCX_GLUE_REGS_BASE 0x400A5000
#define NPCX_GLUE_PSL_CTS REG8(NPCX_GLUE_REGS_BASE + 0x027)

/* PSL wake source mask for AC */
#define WAKE_SOURCE_AC_MASK 0x4

int board_check_hibernate_wake_source_ac(void)
{
	int ret = 0;
	if (!(system_get_reset_flags() & EC_RESET_FLAG_HIBERNATE))
		return ret;

	ret = NPCX_GLUE_PSL_CTS & 0xf;
	CPRINTS("PSL_CTS: 0x%x", ret);

	return ret & WAKE_SOURCE_AC_MASK;
}

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

void reset_all_passthru_pmic_signal(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_pmic_acok), 0);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_pmic_lid_open_od), 0);
}
