/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>

#include "charger.h"
#include "charge_state.h"
#include "charger_profile_override.h"
#include "common.h"
#include "config.h"
#include "hooks.h"
#include "util.h"
#include "temp_sensor.h"
#include "temp_sensor/temp_sensor.h"

#define ADC_DEVICE_NODE DT_NODELABEL(adc0)
#define CHARGER_TEMP TEMP_SENSOR_ID(DT_NODELABEL(temp_charger))
#define NUM_CURRENT_LEVELS ARRAY_SIZE(current_table)
#define TEMP_THRESHOLD 55
#define KEEP_TIME 5

static int fake_voltage;
static int current_level;
static int request_current;

/* Limit charging current table : 3600/3000/2400/1800
 * note this should be in descending order.
 */
static uint16_t current_table[] = {
	3600,
	3000,
	2400,
	1800,
};

static int setup_faketemp(int fake_voltage)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	int emul_temp, temp;

	adc_emul_const_value_set(adc_dev, temp_sensors[CHARGER_TEMP].idx,
				 fake_voltage);
	temp_sensor_read(CHARGER_TEMP, &temp);
	emul_temp = K_TO_C(temp);
	return emul_temp;
}

static void current_update(void)
{
	int temp;
	static uint8_t uptime;
	static uint8_t dntime;

	temp = setup_faketemp(fake_voltage);

	if (temp >= TEMP_THRESHOLD) {
		dntime = 0;
		if (uptime < KEEP_TIME) {
			uptime++;
		} else {
			uptime = 0;
			current_level++;
		}
	} else if (current_level != 0 && temp < TEMP_THRESHOLD) {
		uptime = 0;
		if (dntime < KEEP_TIME) {
			dntime++;
		} else {
			dntime = 0;
			current_level--;
		}
	} else {
		uptime = 0;
		dntime = 0;
	}
	if (current_level > NUM_CURRENT_LEVELS) {
		current_level = NUM_CURRENT_LEVELS;
	}
}

int charger_profile_override_test(void)
{
	static int request_current;

	if (current_level != 0) {
		request_current = current_table[current_level - 1];
		return request_current;
	}
	return 0;
}

ZTEST(temp_tentacruel, test_decrease_current)
{
	static int count;
	static uint16_t test_current_table[5];

	fake_voltage = 376;
	count = 0;

	/* Confirm temperature is equal or upper than threshold */
	zassert_between_inclusive(setup_faketemp(fake_voltage), TEMP_THRESHOLD,
				  100);
	for (int i = 1; i < 26; i++) {
		hook_notify(HOOK_SECOND);
		current_update();
		if (i % 5 == 0) {
			test_current_table[count] =
				charger_profile_override_test();
			count++;
		}
	}
	/* test_current_table[0] is original current*/
	zassert_equal(current_table[0], test_current_table[1], NULL);
	zassert_equal(current_table[1], test_current_table[2], NULL);
	zassert_equal(current_table[2], test_current_table[3], NULL);
	zassert_equal(current_table[3], test_current_table[4], NULL);
}
ZTEST(temp_tentacruel, test_increase_current)
{
	static int count;
	static uint16_t test_current_table[5];

	fake_voltage = 376;
	count = 0;

	for (int i = 1; i < 26; i++) {
		hook_notify(HOOK_SECOND);
		current_update();
	}
	/* Confirm current is 1800 mA */
	zassert_equal(current_table[3], charger_profile_override_test(), NULL);

	fake_voltage = 411;
	/* Confirm temperature under threshold  */
	zassert_between_inclusive(setup_faketemp(fake_voltage), 0,
				  TEMP_THRESHOLD - 1);
	for (int i = 1; i < 26; i++) {
		hook_notify(HOOK_SECOND);
		current_update();
		if (i % 5 == 0) {
			test_current_table[count] =
				charger_profile_override_test();
			count++;
		}
	}
	/* test_current_table[4] is original current*/
	zassert_equal(current_table[3], test_current_table[0], NULL);
	zassert_equal(current_table[2], test_current_table[1], NULL);
	zassert_equal(current_table[1], test_current_table[2], NULL);
	zassert_equal(current_table[0], test_current_table[3], NULL);
}
static void temp_tentacruel_before(void *fixture)
{
	fake_voltage = 0;
	request_current = 0;
	current_level = 0;
}

ZTEST_SUITE(temp_tentacruel, NULL, NULL, temp_tentacruel_before, NULL, NULL);
