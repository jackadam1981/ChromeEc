/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

#define CHARGING_CURRENT_MA_SAFE 5000

static int thermals[5];
static int thermal_cyc;
static int charger_temp_ave_bef;

int charger_profile_override(struct charge_state_data *curr)
{
	int charger_temp, charger_temp_c, charger_temp_ave;
	int current;
	int charger_temp_sum = 0;
	enum power_state chipset_state = power_get_state();

	/*
	 * Keep track of battery temperature range:
	 *
	 *     ZONE_0  ZONE_1   ZONE_2  ZONE_3
	 * --->------>-------->-------->------>--- Temperature (C)
	 *    0      50       53       56     80
	 *     ZONE_0  ZONE_1   ZONE_2  ZONE_3
	 * ---<------<--------<--------<------<--- Temperature (C)
	 *    0      45        50       54     80
	 */
	enum {
		TEMP_ZONE_0, /* not limit */
		TEMP_ZONE_1, /* 2500mA */
		TEMP_ZONE_2, /* 1800mA */
		TEMP_ZONE_3, /* 1000mA */
		TEMP_ZONE_COUNT,
		TEMP_OUT_OF_RANGE = TEMP_ZONE_COUNT /* Not charging */
	} temp_zone;

	if (!(curr->batt.flags & BATT_FLAG_RESPONSIVE))
		return 0;

	current = curr->requested_current;

	temp_sensor_read(
		TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(charger_bc12_port1)),
		&charger_temp);

	charger_temp_c = K_TO_C(charger_temp);

	if ((charger_temp_c >= 125) || (charger_temp_c <= -30))
		return 0;

	thermals[thermal_cyc] = charger_temp_c;
	thermal_cyc = (thermal_cyc + 1) % 5;
	for (int i = 0; i < 5; i++)
		charger_temp_sum += thermals[i];

	charger_temp_ave_bef = charger_temp_ave;
	charger_temp_ave = (charger_temp_sum + 2.5) / 5;

	if (thermals[4]) {
		if (chipset_state != POWER_S0) {
		if ((curr->batt.flags & BATT_FLAG_BAD_TEMPERATURE) ||
		    (charger_temp_ave >= 90))
			temp_zone = TEMP_OUT_OF_RANGE;
		else
			temp_zone = TEMP_ZONE_0;
	} else {
		if (charger_temp_ave_bef <= charger_temp_ave) {
			if ((curr->batt.flags &
			     BATT_FLAG_BAD_TEMPERATURE) ||
			    (charger_temp_ave >= 90))
				temp_zone = TEMP_OUT_OF_RANGE;
			else if (charger_temp_ave >= 85)
				temp_zone = TEMP_ZONE_3;
			else if (charger_temp_ave >= 75)
				temp_zone = TEMP_ZONE_2;
			else if (charger_temp_ave >= 65)
				temp_zone = TEMP_ZONE_1;
			else
				temp_zone = TEMP_ZONE_0;
		} else {
			if ((curr->batt.flags &
				 BATT_FLAG_BAD_TEMPERATURE) ||
				(charger_temp_ave >= 90))
				temp_zone = TEMP_OUT_OF_RANGE;
			else if (charger_temp_ave < 60)
				temp_zone = TEMP_ZONE_0;
			else if (charger_temp_ave < 66)
				temp_zone = TEMP_ZONE_1;
			else if (charger_temp_ave < 76)
				temp_zone = TEMP_ZONE_2;
			else if (charger_temp_ave < 86)
				temp_zone = TEMP_ZONE_3;
			}
		}
	}

	switch (temp_zone) {
	case TEMP_ZONE_0:
		current = CHARGING_CURRENT_MA_SAFE;
		break;
	case TEMP_ZONE_1:
		current = 2500;
		break;
	case TEMP_ZONE_2:
		current = 1800;
		break;
	case TEMP_ZONE_3:
		current = 1000;
		break;
	case TEMP_OUT_OF_RANGE:
		/* Don't charge if outside of allowable temperature range */
		current = 0;
		curr->batt.flags &= ~BATT_FLAG_WANT_CHARGE;
		if (curr->state != ST_DISCHARGE)
			curr->state = ST_IDLE;
		break;
	}

	curr->requested_current = MIN(curr->requested_current, current);

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
