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
static uint16_t temp_charger_voltage[] = {
	731, 708, 682, 653, 622, 589, 554, 519, 483, 446, 411,
	376, 343, 312, 284, 257, 232, 209, 188, 169, 152,
};

static void set_adc_emul_read_voltage(int voltage, const struct device *adc_dev,
				      uint8_t channel_id)
{
	zassert_ok(adc_emul_const_value_set(adc_dev, channel_id, voltage));
}

static void wait_heat_stable(struct charge_state_data *curr)
{
	for (int i = 0; i < 5; i++) {
		curr->requested_current = DEFAULT_CURRENT;
		zassert_ok(charger_profile_override(curr));
	}
}

ZTEST(temp_current, test_current_limit_in_each_zone)
{
	const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));
	struct charge_state_data curr;
	int temp_ave_voltage, charger_voltage;
	uint8_t charger_adc_channel =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_charger));
	uint8_t lcd_adc_channel =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_temp_sensor_1));

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	set_adc_emul_read_voltage(411, adc_dev, charger_adc_channel);
	set_adc_emul_read_voltage(606, adc_dev, lcd_adc_channel);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	set_adc_emul_read_voltage(temp_charger_voltage[16], adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	power_set_state(POWER_S0);
	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 45, lcd_ntc = 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = DEFAULT_CURRENT;
	set_adc_emul_read_voltage(temp_charger_voltage[9], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(446, adc_dev, lcd_adc_channel);
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	curr.requested_current = DEFAULT_CURRENT;
	set_adc_emul_read_voltage(temp_charger_voltage[17], adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 45, lcd_ntc < 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	set_adc_emul_read_voltage(temp_charger_voltage[9], adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	set_adc_emul_read_voltage(temp_charger_voltage[18], adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc < 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	set_adc_emul_read_voltage(temp_charger_voltage[10], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(1200, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc < 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	set_adc_emul_read_voltage(temp_charger_voltage[17], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(1200, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 45, lcd_ntc > 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	set_adc_emul_read_voltage(temp_charger_voltage[9], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(400, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc > 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	set_adc_emul_read_voltage(temp_charger_voltage[17], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(400, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	set_adc_emul_read_voltage(temp_charger_voltage[10], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(606, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	set_adc_emul_read_voltage(temp_charger_voltage[17], adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(606, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 45, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[9] - temp_charger_voltage[10]) / 5;
	charger_voltage = temp_charger_voltage[10] + temp_ave_voltage;
	temp_ave_voltage =
		(temp_charger_voltage[10] - temp_charger_voltage[11]) / 5;
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(446, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	wait_heat_stable(&curr);
	zassert_equal(curr.requested_current, DEFAULT_CURRENT);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 50, lcd_ntc = 43
	 */
	charger_voltage = temp_charger_voltage[10] - (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	set_adc_emul_read_voltage(446, adc_dev, lcd_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 2500);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 53, lcd_ntc = 43
	 */
	charger_voltage = temp_charger_voltage[11] + temp_ave_voltage;
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 1800);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 56, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[11] - temp_charger_voltage[12]) / 5;
	charger_voltage = temp_charger_voltage[11] - (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 1000);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 80, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[16] - temp_charger_voltage[17]) / 5;
	charger_voltage = temp_charger_voltage[16] - (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	curr.requested_current = DEFAULT_CURRENT;
	charger_profile_override(&curr);
	zassert_equal(curr.requested_current, 0);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 79, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[15] - temp_charger_voltage[16]) / 5;
	charger_voltage = temp_charger_voltage[16] + (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	zassert_equal(curr.requested_current, 1000);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 54, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[10] - temp_charger_voltage[11]) / 5;
	charger_voltage = temp_charger_voltage[11] + (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(temp_charger_voltage[11], adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	zassert_equal(curr.requested_current, 1800);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 50, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[9] - temp_charger_voltage[10]) / 5;
	charger_voltage = temp_charger_voltage[10] + temp_ave_voltage;
	set_adc_emul_read_voltage(temp_charger_voltage[10], adc_dev,
				  charger_adc_channel);
	charger_profile_override(&curr);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	zassert_equal(curr.requested_current, 2500);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger > 45, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[8] - temp_charger_voltage[9]) / 5;
	charger_voltage = temp_charger_voltage[9] + (temp_ave_voltage * 2);
	set_adc_emul_read_voltage(charger_voltage, adc_dev,
				  charger_adc_channel);
	wait_heat_stable(&curr);
	zassert_equal(curr.requested_current, 5000);
}

ZTEST_SUITE(temp_current, NULL, NULL, NULL, NULL, NULL);
