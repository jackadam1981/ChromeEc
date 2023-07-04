/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_cbi.h"
#include "gpio_signal.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VOID_FUNC(fan_set_count, int);
FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);

void fan_init(void);

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

static void joxer_fan_before(void *fixture)
{
	ARG_UNUSED(fixture);
    RESET_FAKE(fan_set_count);
    RESET_FAKE(cros_cbi_get_fw_config);
}

ZTEST_SUITE(joxer_fan, NULL, NULL, joxer_fan_before, NULL, NULL);

ZTEST(joxer_fan, test_fan_present)
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

ZTEST(joxer_fan, test_fan_absent)
{
	int flags;

	cros_cbi_get_fw_config_fake.custom_fake = get_fan_config_absent;
	fan_init();

	zassert_equal(fan_set_count_fake.call_count, 1,
		      "function actually called %d times",
		      fan_set_count_fake.call_count);
	zassert_equal(fan_set_count_fake.arg0_val, 0, "parameter value was %d",
		      fan_set_count_fake.arg0_val);

	/* Fan enable is left unconfigured */
	zassert_ok(gpio_pin_get_config_dt(
		GPIO_DT_FROM_NODELABEL(gpio_fan_enable), &flags));
	zassert_equal(flags, 0, "actual GPIO flags were %#x", flags);
}

ZTEST(joxer_fan, test_fan_cbi_error)
{
	int flags;

	cros_cbi_get_fw_config_fake.return_val = EINVAL;
	fan_init();

	zassert_equal(fan_set_count_fake.call_count, 0);
	zassert_ok(gpio_pin_get_config_dt(
		GPIO_DT_FROM_NODELABEL(gpio_fan_enable), &flags));
	zassert_equal(flags, 0, "actual GPIO flags were %#x", flags);
}
