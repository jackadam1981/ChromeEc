/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Tests for cros_button.c
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "drivers/gpio_debounce.h"
#include "common.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"

LOG_MODULE_REGISTER(gpio_debounce_cfg_test, LOG_LEVEL_INF);

const struct device *test_gpio_debounce_dev =
	DEVICE_DT_GET(DT_NODELABEL(test_button));

int stub_gpio_debounce_state;
int stub_get_gpio_debounce_state(const struct device *d, gpio_pin_t p)
{
	ARG_UNUSED(d);
	ARG_UNUSED(p);
	return stub_gpio_debounce_state;
}

FAKE_VALUE_FUNC(int, stub_gpio_pin_get, const struct device *, gpio_pin_t);
FAKE_VALUE_FUNC(int, stub_gpio_pin_get_raw, const struct device *, gpio_pin_t);

#define BUTTON_CFG_LIST(FAKE)                \
	{                                    \
		FAKE(stub_gpio_pin_get);     \
		FAKE(stub_gpio_pin_get_raw); \
	}

static void test_gpio_debounce_cfg_reset(void)
{
	BUTTON_CFG_LIST(RESET_FAKE);

	FFF_RESET_HISTORY();

	stub_gpio_debounce_state = 0;
	stub_gpio_pin_get_fake.custom_fake = gpio_pin_get;
	stub_gpio_pin_get_raw_fake.custom_fake = gpio_pin_get_raw;
}

static void cros_gpio_debounce__rule(const struct ztest_unit_test *test,
				     void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	test_gpio_debounce_cfg_reset();
}

ZTEST_RULE(cros_gpio_debounce_rule, cros_gpio_debounce__rule,
	   cros_gpio_debounce__rule);

/**
 * Make sure mocks are setup before HOOK(HOOK_PRIO_INIT_POWER_BUTTON) runs
 * otherwise unexpected calls to mocks above occur prevent default
 * gpio_pin_get behavior
 */
DECLARE_HOOK(HOOK_INIT, test_gpio_debounce_cfg_reset, HOOK_PRIO_FIRST);

/**
 * @brief Test Suite: Verifies gpio_debounce_config functionality.
 */
ZTEST_SUITE(cros_gpio_debounce, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);

/**
 * @brief TestPurpose: Verify gpio_debounce_config initialization.
 *
 */
ZTEST(cros_gpio_debounce, test_gpio_debounce_config)
{
	struct gpio_debounce_config button;

	gpio_debounce_common_get_cfg(test_gpio_debounce_dev, &button);

	LOG_INF("button = {%s, %d, {%d, 0x%X}, %d}\n",
		test_gpio_debounce_dev->name, button.type, button.spec.pin,
		button.spec.dt_flags, button.debounce_us);

	zassert_equal(button.type, 1, NULL);
	zassert_equal(button.spec.pin, 27, NULL);
	zassert_equal(button.debounce_us, 30000, NULL);
}

/**
 * @brief TestPurpose: Verify gpio_debounce_config pressed raw.
 *
 */
ZTEST(cros_gpio_debounce, test_gpio_debounce_pressed)
{
	stub_gpio_pin_get_fake.custom_fake = stub_get_gpio_debounce_state;

	stub_gpio_debounce_state = 1;
	zassert_equal(1, gpio_debounce_common_get_pin(test_gpio_debounce_dev));

	stub_gpio_debounce_state = 0;
	zassert_equal(0, gpio_debounce_common_get_pin(test_gpio_debounce_dev));

	stub_gpio_debounce_state = -1;
	zassert_equal(0, gpio_debounce_common_get_pin(test_gpio_debounce_dev));
}

/**
 * @brief TestPurpose: Verify gpio_debounce_config pressed raw.
 *
 */
ZTEST(cros_gpio_debounce, test_gpio_debounce_pressed_raw)
{
	stub_gpio_pin_get_raw_fake.custom_fake = stub_get_gpio_debounce_state;

	stub_gpio_debounce_state = 1;
	zassert_equal(1,
		      gpio_debounce_common_get_pin_raw(test_gpio_debounce_dev));

	stub_gpio_debounce_state = 0;
	zassert_equal(0,
		      cros_gpio_debounce_get_pin_raw(test_gpio_debounce_dev));

	stub_gpio_debounce_state = -1;
	zassert_equal(0,
		      gpio_debounce_common_get_pin_raw(test_gpio_debounce_dev));
}

/**
 * @brief TestPurpose: Verify button debounce.
 *
 */
ZTEST(cros_gpio_debounce, test_gpio_debounce_debounce)
{
	const uint32_t expected_time = 30000;
	uint32_t debounce_time_us = 30000;

	zassert_ok(gpio_debounce_common_get_debounce_us(test_gpio_debounce_dev,
							&debounce_time_us),
		   NULL);
	zassert_equal(expected_time, debounce_time_us, NULL);
}

/**
 * @brief TestPurpose: Verify button interrupt.
 *
 */
bool gpio_debounce_interrupt_called;
void test_gpio_debounce_cb_handler(const struct device *dev,
				   struct gpio_callback *cbdata, uint32_t pins)
{
	LOG_ERR("Button %s pressed", dev->name);
	gpio_debounce_interrupt_called = true;
}

ZTEST(cros_gpio_debounce, test_gpio_debounce_interrupt)
{
	struct gpio_debounce_config cfg;

	gpio_debounce_common_get_cfg(test_gpio_debounce_dev, &cfg);

	gpio_debounce_interrupt_called = false;

	zassert_ok(
		gpio_debounce_common_disable_interrupt(test_gpio_debounce_dev),
		NULL);
	gpio_pin_set_raw(cfg.spec.port, cfg.spec.pin, 0);
	gpio_pin_set_raw(cfg.spec.port, cfg.spec.pin, 1);
	zassert_equal(gpio_debounce_interrupt_called, false);

	zassert_ok(gpio_debounce_common_enable_interrupt(
			   test_gpio_debounce_dev,
			   test_gpio_debounce_cb_handler),
		   NULL);
	gpio_pin_set_raw(cfg.spec.port, cfg.spec.pin, 0);
	gpio_pin_set_raw(cfg.spec.port, cfg.spec.pin, 1);
	zassert_equal(gpio_debounce_interrupt_called, true);
}
