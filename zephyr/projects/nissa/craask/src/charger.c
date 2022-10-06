/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>

#include "battery.h"
#include "charge_state_v2.h"
#include "charger.h"
#include "charger/isl923x_public.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "temp_sensor/temp_sensor.h"
#include "usb_pd.h"
#include "util.h"
#include "nissa_common.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

int extpower_is_present(void)
{
	int port;
	int rv;
	bool acok;

	for (port = 0; port < board_get_usb_pd_port_count(); port++) {
		rv = raa489000_is_acok(port, &acok);
		if ((rv == EC_SUCCESS) && acok)
			return 1;
	}

	return 0;
}

/*
 * Craask does not have a GPIO indicating whether extpower is present,
 * so detect using the charger(s).
 */
__override void board_check_extpower(void)
{
	static int last_extpower_present;
	int extpower_present = extpower_is_present();

	if (last_extpower_present ^ extpower_present)
		extpower_handle_update(extpower_present);

	last_extpower_present = extpower_present;
}

__override void board_hibernate(void)
{
	/* Shut down the chargers */
	if (board_get_usb_pd_port_count() == 2)
		raa489000_hibernate(CHARGER_SECONDARY, true);
	raa489000_hibernate(CHARGER_PRIMARY, true);
	LOG_INF("Charger(s) hibernated");
	cflush();
}

struct temp_chg_step {
	int low; /* temp thershold ('C) to lower level */
	int high; /* temp thershold ('C) to higher level */
	int current; /* charging limitation (mA) */
};

static const struct temp_chg_step temp_chg_table[] = {
	{ .low = 0, .high = 52, .current = 3000 }, /* Lv0: normal charge */
	{ .low = 48, .high = 54, .current = 1500 },
	{ .low = 52, .high = 56, .current = 800 },
	{ .low = 55, .high = 100, .current = 0 },
};
#define NUM_TEMP_CHG_LEVELS ARRAY_SIZE(temp_chg_table)

int charger_profile_override(struct charge_state_data *curr)
{
	static int current_level;
	int charger_temp, charger_temp_c;

	if (curr->state != ST_CHARGE)
		return 0;

	/* Charge current control depends on temp if the system is on */
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		temp_sensor_read(TEMP_SENSOR_ID(DT_NODELABEL(charger)),
				 &charger_temp);
		charger_temp_c = K_TO_C(charger_temp);

		if (charger_temp_c <= temp_chg_table[current_level].low)
			current_level--;
		else if (charger_temp_c >= temp_chg_table[current_level].high)
			current_level++;

		if (current_level < 0)
			current_level = 0;

		if (current_level >= NUM_TEMP_CHG_LEVELS)
			current_level = NUM_TEMP_CHG_LEVELS - 1;

		curr->requested_current =
			MIN(curr->requested_current,
			    temp_chg_table[current_level].current);
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
