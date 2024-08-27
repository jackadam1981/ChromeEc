/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_board_info.h"
#include "cros_cbi.h"
#include "ec_commands.h"
#include "fan.h"
#include "host_command.h"
#include "thermal.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define CHIPSET_STATE_NOT_ON 0
#define CHIPSET_STATE_ON 1

FAKE_VOID_FUNC(host_set_single_event, enum host_event_code);
FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(int, chipset_in_state, int);

void fan_init(void);

static void test_before(void *fixture)
{
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(chipset_in_state);
}
ZTEST_SUITE(jubilant_fan, NULL, NULL, test_before, NULL, NULL);

static int thermal_solution;

static int cbi_get_thermal_fw_config(enum cbi_fw_config_field_id field,
				     uint32_t *value)
{
	zassert_equal(field, FW_THERMAL);
	*value = thermal_solution;
	return 0;
}

static int chipset_state;

static int chipset_in_state_mock(int state_mask)
{
	if (state_mask & chipset_state)
		return 1;

	return 0;
}

ZTEST(jubilant_fan, test_fan_table)
{
	int temp = 35;

	/* Test fan table for default table */
	cros_cbi_get_fw_config_fake.custom_fake = cbi_get_thermal_fw_config;
	thermal_solution = FW_THERMAL_passive;
	fan_init();

	/* Turn on fan when chipset state on. */
	chipset_in_state_fake.custom_fake = chipset_in_state_mock;
	chipset_state = CHIPSET_STATE_ON;

	/* level_0 */
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 0);

	/* level_1 */
	temp = 37;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 2500);

	/* level_2 */

	temp = 40;

	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 2900);

	/* level_3 */
	temp = 43;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 3300);

	/* level_4 */
	temp = 46;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 3650);

	/* level_5 */
	temp = 49;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 4100);

	/* level_6 */
	temp = 52;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 4500);

	/* level_7 */
	temp = 60;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 5300);

	/* level_8 */
	temp = 66;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 5800);

	/* decrease temp to level_7 */
	temp = 59;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 5300);

	/* decrease temp to level_6 */
	temp = 51;

	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 4500);

	/* decrease temp to level_5 */
	temp = 48;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 4100);

	/* decrease temp to level_4 */
	temp = 45;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 3650);

	/* decrease temp to level_3 */
	temp = 42;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 3300);

	/* decrease temp to level_2 */
	temp = 39;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 2900);

	/* decrease temp to level_1 */
	temp = 36;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 2500);

	/* decrease temp to level_0 */
	temp = 34;
	board_override_fan_control(0, &temp);
	zassert_equal(fan_get_rpm_mode(0), 1);
	zassert_equal(fan_get_rpm_target(0), 0);
}
