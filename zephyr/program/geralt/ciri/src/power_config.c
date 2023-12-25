/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "temp_sensor/temp_sensor.h"
#include "usb_pd.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

#define TEMP_OVER_HIGH_TIME 5
#define CHARGE_CURRENT_LIMIT 2000
static int limit_current_1;
static int limit_current_2;
int charger_profile_override(struct charge_state_data *curr)
{
	int i, current;
	/* battery temp in 0.1 deg C */
	int charger_temp, charger_temp_c;
	int pp500_temp, pp5500_temp_c;
	int charge_port;
	static timestamp_t deadline;

	current = curr->requested_current;

	temp_sensor_read(
		TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(charger_temp_thermistor)),
		&charger_temp);

	temp_sensor_read(
		TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(pp5000_z1_temp_thermistor)),
		&pp500_temp);

	charger_temp_c = K_TO_C(charger_temp);
	pp5500_temp_c = K_TO_C(pp500_temp);

	if (extpower_is_present()) {
		if ((charger_temp_c >= 65) && (pp5500_temp_c >= 56)) {
			if (deadline.val == 0)
				deadline.val = get_time().val +
					       TEMP_OVER_HIGH_TIME * SECOND;
			else if (timestamp_expired(deadline, NULL)) {
				limit_current_1 = 1;
			}
		} else {
			deadline.val = 0;
		}

		if ((charger_temp_c <= 60) && (pp5500_temp_c <= 53)) {
			limit_current_1 = 0;
		}

		charge_port = charge_manager_get_active_charge_port();
		for (i = 0; i < board_get_usb_pd_port_count(); i++) {
			if (i == charge_port) {
				continue;
			}

			if (pd_get_power_role(i) == PD_ROLE_SOURCE) {
				limit_current_2 = 1;
			} else {
				limit_current_1 = 0;
				limit_current_2 = 0;
				deadline.val = 0;
			}
		}
	} else {
		limit_current_1 = 0;
		limit_current_2 = 0;
		deadline.val = 0;
	}

	if (limit_current_1 && limit_current_2) {
		curr->requested_current = MIN(current, CHARGE_CURRENT_LIMIT);
	} else {
		curr->requested_current = current;
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
