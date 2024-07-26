/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger.h"
#include "charger_profile_override.h"
#include "common.h"
#include "config.h"
#include "hooks.h"
#include "temp_sensor.h"
#include "temp_sensor/temp_sensor.h"
#include "util.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define ADC_DEVICE_NODE DT_NODELABEL(adc0)
#define CHARGER_TEMP TEMP_SENSOR_ID(DT_NODELABEL(temp_charger))
#define ORIGINAL_CURRENT 5000

struct charge_state_data curr;
static int fake_voltage;
int count;

/* Limit charging current table : 3600/3000/2400/1800
 * note this should be in descending order.
 */

struct current_table_struct {
	int temperature;
	int current;
};

static const struct current_table_struct current_table[] = {
	{ 0, 2554 },
	{ 55, 1400 },
	{ 57, 365 },
};

#define CURRENT_LEVELS ARRAY_SIZE(current_table)

int setup_faketemp(int fake_voltage)
{
	const struct device *adc_dev = DEVICE_DT_GET(ADC_DEVICE_NODE);
	const uint8_t channel_id =
		DT_IO_CHANNELS_INPUT(DT_NODELABEL(adc_charger));
	int emul_temp;

	emul_temp = adc_emul_const_value_set(adc_dev, channel_id, fake_voltage);
	return emul_temp;
}

static void ignore_first_minute(void)
{
	for (int uptime_time = 0; uptime_time < 60; uptime_time++) {
		hook_notify(HOOK_SECOND);
	}
}

ZTEST(temp_veluza, test_decrease_current)
{
	fake_voltage = 411;
	curr.batt.flags |= BATT_FLAG_RESPONSIVE;
	count = 0;

	setup_faketemp(fake_voltage);
	/* Calculate per minute temperature.
	 * It's expected low temperature when the first 60 seconds.
	 */
	ignore_first_minute();
	for (int uptime_time = 1; uptime_time < 26; uptime_time++) {
		hook_notify(HOOK_SECOND);
		curr.requested_current = ORIGINAL_CURRENT;
		charger_profile_override(&curr);
		if (uptime_time % 6 == 0) {
			count++;
		}
	}
}

ZTEST(temp_veluza, test_increase_current)
{
	fake_voltage = 410;
	curr.batt.flags |= BATT_FLAG_RESPONSIVE;
	count = 2;

	int value = setup_faketemp(fake_voltage);

	for (int uptime_time = 0; uptime_time < 500; uptime_time++) {
		hook_notify(HOOK_SECOND);
		curr.requested_current = ORIGINAL_CURRENT;
		if (uptime_time % 5 == 0) {
			if (curr.requested_current == ORIGINAL_CURRENT) {
				zassert_equal(ORIGINAL_CURRENT,
					      curr.requested_current, NULL);
			} else if (value < current_table[count].temperature) {
				charger_profile_override(&curr);
				zassert_equal(curr.requested_current,
					      current_table[count].current);
				count--;
			}
		}
		if (count < 0)
			count = 0;
	}
}

ZTEST(temp_veluza, test_decrease_current_level)
{
	fake_voltage = 200;
	count = 0;
	curr.requested_current = ORIGINAL_CURRENT;
	int value = setup_faketemp(fake_voltage);

	for (int uptime_time = 0; uptime_time < 500; uptime_time++) {
		hook_notify(HOOK_SECOND);
		charger_profile_override(&curr);

		if (uptime_time % 5 == 0) {
			if (curr.requested_current == ORIGINAL_CURRENT) {
				zassert_equal(ORIGINAL_CURRENT,
					      curr.requested_current, NULL);
			} else if (value >=
				   current_table[count + 1].temperature) {
				charger_profile_override(&curr);
				zassert_equal(curr.requested_current,
					      current_table[count].current);
				count++;
			}
		}
		if (count > 2)
			count = 2;
	}
}

ZTEST(temp_veluza, test_battery_no_response)
{
	int rv;

	curr.batt.flags &= ~BATT_FLAG_RESPONSIVE;
	rv = charger_profile_override(&curr);
	zassert_equal(rv, 0);
}

ZTEST(temp_veluza, test_charger_profile_override_get_param)
{
	int rv;

	rv = charger_profile_override_get_param(0, 0);

	zassert_equal(rv, 3);
}

ZTEST(temp_veluza, test_charger_profile_override_set_param)
{
	int rv;

	rv = charger_profile_override_set_param(0, 0);

	zassert_equal(rv, 3);
}

ZTEST_SUITE(temp_veluza, NULL, NULL, NULL, NULL, NULL);
