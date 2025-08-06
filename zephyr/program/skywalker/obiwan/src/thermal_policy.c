/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor/temp_sensor.h"

#define POLL_COUNT 3
#define LIMIT_LEVELS 4

static uint8_t limit_level = 0;
static uint8_t trigger_cnt = 0;
static uint8_t release_cnt = 0;

typedef enum {
	LIMIT_NONE = 9999,
	LIMIT_3500 = 3500,
	LIMIT_3000 = 3000,
	LIMIT_2000 = 2000
} charge_limit_t;

typedef struct {
	uint8_t trigger_temp;
	uint8_t release_temp;
} temp_limit_t;

static const charge_limit_t limit_table[LIMIT_LEVELS] = {
	LIMIT_NONE, LIMIT_3500, LIMIT_3000, LIMIT_2000
};

static const temp_limit_t charge_temp_limits[LIMIT_LEVELS - 1] = { { 47, 43 },
								   { 52, 47 },
								   { 56, 52 } };

static void update_charge_limit(void)
{
	int charger_temp_k, charger_temp;

	if (power_get_state() != POWER_S0) {
		limit_level = 0;
		trigger_cnt = 0;
		release_cnt = 0;
		return;
	}

	temp_sensor_read(TEMP_SENSOR_ID_BY_DEV(DT_NODELABEL(temp_charger)),
			 &charger_temp_k);

	charger_temp = K_TO_C(charger_temp_k);

	if (limit_level < LIMIT_LEVELS - 1) {
		temp_limit_t chg = charge_temp_limits[limit_level];

		if (charger_temp >= chg.trigger_temp) {
			if (++trigger_cnt >= POLL_COUNT) {
				limit_level++;
				trigger_cnt = 0;
				release_cnt = 0;
			}
		} else {
			trigger_cnt = 0;
		}
	}

	if (limit_level > 0) {
		temp_limit_t chg = charge_temp_limits[limit_level - 1];

		if (charger_temp < chg.release_temp) {
			if (++release_cnt >= POLL_COUNT) {
				limit_level--;
				trigger_cnt = 0;
				release_cnt = 0;
			}
		} else {
			release_cnt = 0;
		}
	}
}
DECLARE_HOOK(HOOK_SECOND, update_charge_limit, HOOK_PRIO_TEMP_SENSOR_DONE);

int charger_profile_override(struct charge_state_data *curr)
{
	curr->requested_current =
		MIN(curr->requested_current, limit_table[limit_level]);

	return EC_SUCCESS;
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
