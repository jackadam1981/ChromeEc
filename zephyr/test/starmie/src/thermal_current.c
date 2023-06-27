/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charger_profile_override.h"
#include "power.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

struct charge_state_data curr;

ZTEST_SUITE(temp_current, NULL, NULL, NULL, NULL, NULL);

void set_adc_emul_read_voltage(int voltage, const struct device *adc_dev,
			       uint8_t channel_id)
{
	zassert_ok(adc_emul_const_value_set(adc_dev, channel_id, voltage));
}

ZTEST(temp_current, test_current_in_s0)
{
	const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));

	set_adc_emul_read_voltage(
		401, adc_dev, DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_charger)));
	set_adc_emul_read_voltage(
		170, adc_dev,
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_temp_sensor_1)));
	power_set_state(POWER_S0);
	curr.batt.flags |= BATT_FLAG_RESPONSIVE;

	charger_profile_override(&curr);

	zassert_equal(curr.requested_current, 2500);
}
