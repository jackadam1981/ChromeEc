/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "charger.h"
#include "charge_state.h"
#include "charger_profile_override.h"
#include "common.h"
#include "config.h"
#include "hooks.h"
#include "util.h"

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

static void current_update_test(void)
{
	int temp;
	static uint8_t uptime;
	static uint8_t dntime;

	temp = fake_temp;

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
	fake_temp = 55;
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
	fake_temp = 55;
	k_sleep(K_SECONDS(25));
	zassert_equal(current_table[3], charger_profile_override_test(), NULL);
	fake_temp = 54;
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
