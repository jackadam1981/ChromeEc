/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "battery.h"
#include "cros_cbi.h"
#include "hooks.h"

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

void fan_init(void);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
FAKE_VOID_FUNC(board_set_active_charge_port, int);
FAKE_VOID_FUNC(fan_set_count, int);

static void nivviks_test_before(void *fixture)
{
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(battery_is_present);
	RESET_FAKE(board_set_active_charge_port);
	RESET_FAKE(fan_set_count);
}

ZTEST_SUITE(nivviks, NULL, NULL, nivviks_test_before, NULL, NULL);

static int get_fan_config_present(enum cbi_fw_config_field_id field,
				  uint32_t *value)
{
	zassert_equal(field, FW_FAN);
	*value = FW_FAN_PRESENT;
	return 0;
}

static int get_fan_config_absent(enum cbi_fw_config_field_id field,
				 uint32_t *value)
{
	zassert_equal(field, FW_FAN);
	*value = FW_FAN_NOT_PRESENT;
	return 0;
}

ZTEST(nivviks, test_fan_present)
{
	int flags;

	cros_cbi_get_fw_config_fake.custom_fake = get_fan_config_present;
	fan_init();

	zassert_equal(fan_set_count_fake.call_count, 0);
	zassert_ok(gpio_pin_get_config_dt(
		GPIO_DT_FROM_NODELABEL(gpio_fan_enable), &flags));
	zassert_equal(flags, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW,
		      "actual GPIO flags were %#x", flags);
}

ZTEST(nivviks, test_fan_absent)
{
	int flags;

	cros_cbi_get_fw_config_fake.custom_fake = get_fan_config_absent;
	fan_init();

	zassert_equal(fan_set_count_fake.call_count, 1,
		      "function actually called %d times",
		      fan_set_count_fake.call_count);
	zassert_equal(fan_set_count_fake.arg0_val, 0,
		      "parameter value was %d",
		      fan_set_count_fake.arg0_val);

	/* Fan enable is left unconfigured */
	zassert_ok(gpio_pin_get_config_dt(
		GPIO_DT_FROM_NODELABEL(gpio_fan_enable), &flags));
	zassert_equal(flags, 0, "actual GPIO flags were %#x", flags);
}

ZTEST(nivviks, test_fan_cbi_error)
{
	int flags;

	cros_cbi_get_fw_config_fake.return_val = EINVAL;
	fan_init();

	zassert_equal(fan_set_count_fake.call_count, 0);
	zassert_ok(gpio_pin_get_config_dt(
		GPIO_DT_FROM_NODELABEL(gpio_fan_enable), &flags));
	zassert_equal(flags, 0, "actual GPIO flags were %#x", flags);
}