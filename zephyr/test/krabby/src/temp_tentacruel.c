/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/devicetree.h>

#include "stdio.h"
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
#define CHARGER_TEMP TEMP_SENSOR_ID(DT_NODELABEL(fake_temp_charger))
#define NUM_CURRENT_LEVELS ARRAY_SIZE(current_table)
#define TEMP_THRESHOLD 55
#define KEEP_TIME 5

static int fake_temp;
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

static int setup_faketemp(int faketemp)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	int emul_temp, temp;

	adc_emul_const_value_set(adc_dev, temp_sensors[CHARGER_TEMP].idx,
				 faketemp);
	temp_sensor_read(CHARGER_TEMP, &temp);
	emul_temp = K_TO_C(temp);
	return emul_temp;
}

static void current_update_test(void)
{
	int temp;
	static uint8_t uptime;
	static uint8_t dntime;

	temp = setup_faketemp(fake_temp);

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

DECLARE_HOOK(HOOK_SECOND, current_update_test, HOOK_PRIO_DEFAULT);

int charger_profile_override_test(void)
{
	static int request_current;

	if (current_level != 0) {
		request_current = current_table[current_level - 1];
		return request_current;
	}
	return 0;
}

ZTEST(temp_tentacruel, test_increase_current_level)
{
	fake_temp = 376;
	k_sleep(K_SECONDS(10));
	zassert_equal(current_table[0], charger_profile_override_test(), NULL);
	k_sleep(K_SECONDS(5));
	zassert_equal(current_table[1], charger_profile_override_test(), NULL);
	k_sleep(K_SECONDS(5));
	zassert_equal(current_table[2], charger_profile_override_test(), NULL);
	k_sleep(K_SECONDS(5));
	zassert_equal(current_table[3], charger_profile_override_test(), NULL);
}
ZTEST(temp_tentacruel, test_decrease_current_level)
{
	fake_temp = 376;
	k_sleep(K_SECONDS(25));
	zassert_equal(current_table[3], charger_profile_override_test(), NULL);
	fake_temp = 411;
	k_sleep(K_SECONDS(10));
	zassert_equal(current_table[2], charger_profile_override_test(), NULL);
	k_sleep(K_SECONDS(5));
	zassert_equal(current_table[1], charger_profile_override_test(), NULL);
	k_sleep(K_SECONDS(5));
	zassert_equal(current_table[0], charger_profile_override_test(), NULL);
}
static void temp_tentacruel_before(void *fixture)
{
	fake_temp = 0;
	request_current = 0;
	current_level = 0;
}

ZTEST_SUITE(temp_tentacruel, NULL, NULL, temp_tentacruel_before, NULL, NULL);
