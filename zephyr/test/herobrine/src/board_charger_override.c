/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger_override.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define ADC_DEVICE_NODE DT_NODELABEL(adc0)
#define CHARGER_TEMP TEMP_SENSOR_ID(DT_NODELABEL(temp_charger))

#define CURRENT_ORIGINAL 5000
#define CURRENT_LIMIT 2000

struct charge_state_data curr;

int setup_faketemp(int voltage)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	const uint8_t channel_id =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_temp_sensor_1));
	int emul_temp;

	emul_temp = adc_emul_const_value_set(adc_dev, channel_id, voltage);
	return emul_temp;
}

/**
 * fake_voltage:
 * 1580 mV = 298K (= 25 C) -> normal charge
 *  690 mV = 328K (= 55 C) -> normal charge
 *  667 mV = 329K (= 56 C) -> current limit
 *  760 mV = 324K (= 51 C) -> current limit
 *  789 mV = 323K (= 50 C) -> normal charge
 */
const int fake_voltage[] = { 1580, 690, 667, 760, 789 };
const int aim_curr[] = { CURRENT_ORIGINAL, CURRENT_ORIGINAL, CURRENT_LIMIT,
			 CURRENT_LIMIT, CURRENT_ORIGINAL };

ZTEST_USER(board_charger_override, test_board_charger_profile_override)
{
	int i, rv;

	curr.batt.flags |= BATT_FLAG_RESPONSIVE;
	curr.state = ST_CHARGE;

	for (i = 0; i < ARRAY_SIZE(fake_voltage); i++) {
		curr.requested_current = CURRENT_ORIGINAL;
		setup_faketemp(fake_voltage[i]);
		rv = board_charger_profile_override(&curr);
		zassert_equal(EC_SUCCESS, rv,
			      "function did not return success");
		printk(" aim: %d, curr.requested_current: %d\n", aim_curr[i],
		       curr.requested_current);
		zassert_equal(aim_curr[i], curr.requested_current,
			      "requested_current not expected");
	}
}

ZTEST_SUITE(board_charger_override, NULL, NULL, NULL, NULL, NULL);
