
/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger.h"
#include "charge_manager.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "util.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

#define INT_RECHECK_MS 10
#define INT_RECHECK_TIMES 500

/* minimum request for turn on S5 power */
#define MIN_POWER_MW_FOR_TURNON_S5_POWER 7500
#define MIN_BAT_PCT_FOR_POWER_ON 1


static void check_s5_power(void);
DECLARE_DEFERRED(check_s5_power);

static int battery_present(void)
{
	return !gpio_pin_get_dt(
		GPIO_DT_FROM_NODELABEL(gpio_ec_batt_pres_odl));
}

static int check_battery_power(void)
{
	int soc;

	if (!battery_present())
		return 0;

	if (battery_get_disconnect_state() != BATTERY_NOT_DISCONNECTED)
		return 0;

	if (battery_state_of_charge_abs(&soc) == EC_SUCCESS &&
	    soc >= MIN_BAT_PCT_FOR_POWER_ON)
		return soc;
	else
		return 0;
}

static void check_s5_power(void)
{
	static int count;
	int batt_power, pd_power;

	batt_power = check_battery_power();

	pd_power = charge_manager_get_power_limit_uw() / 1000;

	if (batt_power != 0 || pd_power >= MIN_POWER_MW_FOR_TURNON_S5_POWER) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), 1);
		LOG_ERR("%s: Turn on S5 power", __func__);
		return;
	}

	if (count++ >= INT_RECHECK_TIMES) {
		LOG_ERR("Failed to turn on S5 power, back to G3");
		chipset_force_shutdown(CHIPSET_SHUTDOWN_INIT);
		return;
	} else
		hook_call_deferred(&check_s5_power_data, INT_RECHECK_MS * MSEC);

	LOG_ERR("(%d, %d)", batt_power, pd_power);
}

/*
 * check board has enough power to turn on the GPIO_EN_PP5000
 */
static void board_s5_power_init(void)
{
	hook_call_deferred(&check_s5_power_data, INT_RECHECK_MS * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_s5_power_init, HOOK_PRIO_POST_I2C);
