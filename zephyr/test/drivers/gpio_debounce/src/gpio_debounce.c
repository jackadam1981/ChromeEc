/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Tests for cros_button.c
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
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

#define GPIO_DEVICE \
	DEVICE_DT_GET(DT_GPIO_CTLR(DT_PATH(named_gpios, test), gpios))
#define TEST_PIN DT_GPIO_PIN(DT_PATH(named_gpios, test), gpios)

/**
 * @brief Test Suite: Verifies gpio_debounce_config functionality.
 */
ZTEST_SUITE(generic_gpio_debounce, drivers_predicate_post_main, NULL, NULL,
	    NULL, NULL);

/**
 * @brief TestPurpose: Verify gpio_debounce_config initialization.
 *
 */
ZTEST(generic_gpio_debounce, test_gpio_debounce_config)
{
	struct gpio_debounce_config button;

	gpio_debounce_get_config(test_gpio_debounce_dev, &button);

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
ZTEST(generic_gpio_debounce, test_gpio_debounce_pressed)
{
	static const struct device *gpio_dev = GPIO_DEVICE;

	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 1));
	zassert_equal(1, gpio_debounce_get_pin(test_gpio_debounce_dev));

	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 0));
	zassert_equal(0, gpio_debounce_get_pin(test_gpio_debounce_dev));
}

/**
 * @brief TestPurpose: Verify gpio_debounce_config pressed raw.
 *
 */
ZTEST(generic_gpio_debounce, test_gpio_debounce_pressed_raw)
{
	static const struct device *gpio_dev = GPIO_DEVICE;

	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 1));
	zassert_equal(1, gpio_debounce_get_pin_raw(test_gpio_debounce_dev));

	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 0));
	zassert_equal(0, gpio_debounce_get_pin_raw(test_gpio_debounce_dev));
}

/**
 * @brief TestPurpose: Verify button debounce.
 *
 */
ZTEST(generic_gpio_debounce, test_gpio_debounce_debounce)
{
	const uint32_t expected_time = 30000;
	uint32_t debounce_time_us = 30000;

	zassert_ok(gpio_debounce_get_debounce_us(test_gpio_debounce_dev,
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

ZTEST(generic_gpio_debounce, test_gpio_debounce_interrupt)
{
	struct gpio_debounce_config cfg;
	static const struct device *gpio_dev = GPIO_DEVICE;

	gpio_debounce_get_config(test_gpio_debounce_dev, &cfg);

	gpio_debounce_interrupt_called = false;

	zassert_ok(gpio_debounce_disable_interrupt(test_gpio_debounce_dev),
		   NULL);
	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 0));
	k_sleep(K_MSEC(1000));
	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 1));
	k_sleep(K_MSEC(1000));
	zassert_equal(gpio_debounce_interrupt_called, false);

	zassert_ok(
		gpio_debounce_enable_interrupt(test_gpio_debounce_dev,
					       test_gpio_debounce_cb_handler),
		NULL);
	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 0));
	k_sleep(K_MSEC(1000));
	zassert_ok(gpio_emul_input_set(gpio_dev, TEST_PIN, 1));
	k_sleep(K_MSEC(1000));
	zassert_equal(gpio_debounce_interrupt_called, true);
}
