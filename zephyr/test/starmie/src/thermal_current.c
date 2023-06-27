/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger.h"
#include "charger_profile_override.h"
#include "chipset.h"
#include "common.h"
#include "config.h"
#include "hooks.h"
#include "power.h"
#include "temp_sensor.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(enum power_state, power_get_state);

enum power_state state_temp;
enum power_state power_get_state_mock(void)
{
	return state_temp;
}

#define ADC_DEVICE_NODE DT_NODELABEL(adc0)
#define CHARGER_TEMP TEMP_SENSOR_ID(DT_NODELABEL(temp_charger))
#define ORIGINAL_CURRENT 5000

/* Limit charging current table : 2500/1800/1000/0
 * note this should be in descending order.
 */
static uint16_t temp_charger_voltage[] = {
	731, 708, 682, 653, 622, 589, 554, 519, 483, 446, 411,
	376, 343, 312, 284, 257, 232, 209, 188, 169, 152,
};

static uint16_t current_table[] = {
	2500,
	1800,
	1000,
	0,
};

struct charge_state_data curr;

/** Simple ADC emulator custom function which always return error */
static int adc_error_func(const struct device *dev, unsigned int channel,
			  void *param, uint32_t *result)
{
	return -EINVAL;
}

void setup_adc(const struct device *adc_dev, int sensor, int voltage)
{
	int temp;

	/* ADC channel of tested sensor return valid value */
	zassert_ok(adc_emul_const_value_set(adc_dev, temp_sensors[sensor].idx,
					    voltage),
		   "adc_emul_const_value_set() failed (sensor %d)", sensor);
	zassert_equal(EC_SUCCESS, temp_sensor_read(sensor, &temp));
	zassert_within(
		temp, 273 + 50, 51,
		"Expected temperature in 0*C-100*C, got %d*C (sensor %d)",
		temp - 273, sensor);
	/* Return error on ADC channel of tested sensor */
	zassert_ok(adc_emul_value_func_set(adc_dev, temp_sensors[sensor].idx,
					   adc_error_func, NULL),
		   "adc_emul_value_func_set() failed (sensor %d)", sensor);
}

void setup_faketemp(int voltage1, int voltage2)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	const uint8_t channel_id1 =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_charger));
	const uint8_t channel_id2 =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_temp_sensor_1));

	adc_emul_const_value_set(adc_dev, channel_id1, voltage1);
	setup_adc(adc_dev, channel_id2, voltage2);
}

void setup_fakefive(int voltage1, int voltage2)
{
	int i;

	for (i = 0; i < 5; i++) {
		curr.requested_current = ORIGINAL_CURRENT;
		setup_faketemp(voltage1, voltage2);
		charger_profile_override(&curr);
	}
}

ZTEST(temp_current, test_placeholder)
{
	int temp_ave_voltage, charger_voltage;

	/* debounce */
	power_get_state_fake.custom_fake = power_get_state_mock;

	/*
	 * curr.batt.flags != BATT_FLAG_RESPONSIVE
	 * charger = 50, lcd_ntc = 43
	 */
	setup_faketemp(411, 446);
	zassert_equal(0, charger_profile_override(&curr), NULL);
	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = ORIGINAL_CURRENT;
	setup_faketemp(411, 606);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	setup_fakefive(temp_charger_voltage[17], 606);
	setup_faketemp(temp_charger_voltage[17], 606);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	state_temp = POWER_S0;
	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 45, lcd_ntc = 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = ORIGINAL_CURRENT;
	setup_faketemp(temp_charger_voltage[9], 446);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	setup_fakefive(temp_charger_voltage[18], 446);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc < 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = ORIGINAL_CURRENT;
	setup_faketemp(temp_charger_voltage[10], 2500);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 50, lcd_ntc < 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	setup_fakefive(temp_charger_voltage[17], 1219);
	setup_faketemp(temp_charger_voltage[17], 1219);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 45, lcd_ntc > 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = ORIGINAL_CURRENT;
	setup_faketemp(temp_charger_voltage[9], 0);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc > 43
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	setup_fakefive(temp_charger_voltage[18], 0);
	setup_faketemp(temp_charger_voltage[18], 0);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE
	 * charger = 50, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE | BATT_FLAG_BAD_TEMPERATURE;
	curr.requested_current = ORIGINAL_CURRENT;
	setup_faketemp(temp_charger_voltage[10], 606);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger = 80, lcd_ntc = 40
	 */
	curr.batt.flags = BATT_FLAG_RESPONSIVE;
	setup_fakefive(temp_charger_voltage[17], 606);
	setup_faketemp(temp_charger_voltage[17], 606);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 45, lcd_ntc = 43
	 */
	setup_fakefive(temp_charger_voltage[9], 446);
	temp_ave_voltage =
		(temp_charger_voltage[9] - temp_charger_voltage[10]) / 5;
	charger_voltage = temp_charger_voltage[10] + temp_ave_voltage;
	temp_ave_voltage =
		(temp_charger_voltage[10] - temp_charger_voltage[11]) / 5;
	setup_fakefive(charger_voltage, 446);
	zassert_equal(ORIGINAL_CURRENT, curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 50, lcd_ntc = 43
	 */
	charger_voltage = temp_charger_voltage[10] - (temp_ave_voltage * 2);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[0], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 53, lcd_ntc = 43
	 */
	charger_voltage = temp_charger_voltage[11] + temp_ave_voltage;
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[1], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 56, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[11] - temp_charger_voltage[12]) / 5;
	charger_voltage = temp_charger_voltage[11] - (temp_ave_voltage * 2);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[2], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 80, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[16] - temp_charger_voltage[17]) / 5;
	charger_voltage = temp_charger_voltage[16] - (temp_ave_voltage * 2);
	setup_fakefive(charger_voltage, 446);
	setup_faketemp(charger_voltage, 446);
	charger_profile_override(&curr);
	zassert_equal(current_table[3], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 79, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[15] - temp_charger_voltage[16]) / 5;
	charger_voltage = temp_charger_voltage[16] + (temp_ave_voltage * 2);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[2], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 54, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[10] - temp_charger_voltage[11]) / 5;
	charger_voltage = temp_charger_voltage[11] + (temp_ave_voltage * 2);
	setup_faketemp(temp_charger_voltage[11], 446);
	charger_profile_override(&curr);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[1], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger -> 50, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[9] - temp_charger_voltage[10]) / 5;
	charger_voltage = temp_charger_voltage[10] + temp_ave_voltage;
	setup_faketemp(temp_charger_voltage[10], 446);
	charger_profile_override(&curr);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(current_table[0], curr.requested_current, NULL);

	/*
	 * curr.batt.flags = BATT_FLAG_RESPONSIVE
	 * charger > 45, lcd_ntc = 43
	 */
	temp_ave_voltage =
		(temp_charger_voltage[8] - temp_charger_voltage[9]) / 5;
	charger_voltage = temp_charger_voltage[9] + (temp_ave_voltage * 2);
	setup_fakefive(charger_voltage, 446);
	zassert_equal(ORIGINAL_CURRENT, curr.requested_current, NULL);
}

ZTEST_SUITE(temp_current, NULL, NULL, NULL, NULL, NULL);
