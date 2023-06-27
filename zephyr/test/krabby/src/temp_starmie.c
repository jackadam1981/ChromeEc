/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger_profile_override.h"
#include "power.h"

#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define DEFAULT_CURRENT 5000

static void set_adc_emul_read_voltage(int voltage, const struct device *adc_dev,
				      uint8_t channel_id)
{
	zassert_ok(adc_emul_const_value_set(adc_dev, channel_id, voltage));
}

static void wait_heat_stable(struct charge_state_data *curr)
{
	for (int i = 0; i < 5; i++) {
		zassert_ok(charger_profile_override(curr));
	}
}

ZTEST(temp_current, test_current_limit_in_each_zone)
{
	const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
	struct charge_state_data curr;
	uint8_t charger_adc_channel =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_charger));
	uint8_t lid_adc_channel =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_temp_sensor_1));

	power_set_state(POWER_S0);
	curr.batt.flags |= BATT_FLAG_RESPONSIVE;
	curr.requested_current = DEFAULT_CURRENT;
	set_adc_emul_read_voltage(401, adc_dev, charger_adc_channel);
	set_adc_emul_read_voltage(170, adc_dev, lid_adc_channel);
	wait_heat_stable(&curr);

	charger_profile_override(&curr);

	zassert_equal(curr.requested_current, 2500);
}

ZTEST_SUITE(temp_current, NULL, NULL, NULL, NULL, NULL);
