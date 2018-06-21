/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "charge_state_v2.h"
#include "battery_smart.h"
#include "gpio.h"

static enum battery_present batt_pres_prev = BP_NOT_SURE;

enum battery_present battery_hw_present(void)
{
	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(GPIO_EC_BATT_PRES_L) ? BP_NO : BP_YES;
}

/*
 * Physical detection of battery.
 */
static enum battery_present battery_check_present_status(void)
{
	enum battery_present batt_pres;

	/* Get the physical hardware status */
	batt_pres = battery_hw_present();

	/*
	 * If the battery is not physically connected, then no need to perform
	 * any more checks.
	 */
	if (batt_pres != BP_YES)
		return batt_pres;

	/*
	 * If the battery is present now and was present last time we checked,
	 * return early.
	 */
	if (batt_pres == batt_pres_prev)
		return batt_pres;

	/* Ensure that battery is Initialized */
	if (battery_get_disconnect_state() != BATTERY_NOT_DISCONNECTED)
		batt_pres = BP_NOT_SURE;

	return batt_pres;
}

enum battery_present battery_is_present(void)
{
	batt_pres_prev = battery_check_present_status();
	return batt_pres_prev;
}

int charger_profile_override(struct charge_state_data *curr)
{
	/*
	 * If the battery is not yet initialized try to wake it by giving
	 * pre-charge current and set the charge state to pre-charge.
	 */
	if (battery_is_present() == BP_NOT_SURE) {
		set_charge_state(ST_PRECHARGE);
		curr->requested_voltage = battery_get_info()->voltage_max;
		curr->requested_current = battery_get_info()->precharge_current;
	}

	return 0;
}

enum ec_status charger_profile_override_get_param(uint32_t param,
						  uint32_t *value)
{
	return EC_RES_INVALID_PARAM;
}

enum ec_status charger_profile_override_set_param(uint32_t param,
						  uint32_t value)
{
	return EC_RES_INVALID_PARAM;
}
