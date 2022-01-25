/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Unit Tests for GPIO.
 */

#include <device.h>

#include <logging/log.h>
#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "ec_tasks.h"
#include "gpio.h"
#include "gpio/gpio.h"
#include "stubs.h"
#include "util.h"
#include "test_state.h"

/**
 * @brief TestPurpose: Verify Zephyr to EC GPIO bitmask conversion.
 *
 * @details
 * Validate Zephyr to EC GPIO bitmask conversion.
 *
 * Expected Results
 *  - GPIO bitmask conversions are successful
 */
ZTEST(gpio, test_convert_from_zephyr_flags)
{
	int retval;
	struct {
		int zephyr_bmask;
		gpio_flags_t expected_ec_bmask;
	} validate[] = {
		{ GPIO_DISCONNECTED, GPIO_FLAG_NONE },
		{ GPIO_OUTPUT_INIT_LOW, GPIO_LOW },
		{ GPIO_OUTPUT_INIT_HIGH, GPIO_HIGH },
		{ GPIO_VOLTAGE_1P8, GPIO_SEL_1P8V },
		{ GPIO_INT_ENABLE, GPIO_FLAG_NONE },
		{ GPIO_INT_ENABLE | GPIO_INT_EDGE, GPIO_FLAG_NONE },
		{ GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_HIGH_1,
		  GPIO_INT_F_RISING },
		{ GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_LOW_0,
		  GPIO_INT_F_FALLING },
		{ GPIO_INT_ENABLE | GPIO_INT_HIGH_1, GPIO_INT_F_HIGH },
		{ GPIO_INT_ENABLE | GPIO_INT_LOW_0, GPIO_INT_F_LOW },
		{ GPIO_OUTPUT_INIT_LOGICAL, 0 },
		{ GPIO_OPEN_DRAIN | GPIO_PULL_UP,
		  GPIO_OPEN_DRAIN | GPIO_PULL_UP },
	};
	int num_tests = ARRAY_SIZE(validate);

	for (int i = 0; i < num_tests; i++) {
		retval = convert_from_zephyr_flags(validate[i].zephyr_bmask);
		zassert_equal(validate[i].expected_ec_bmask, retval,
			      "Expected 0x%08X, returned 0x%08X.",
			      validate[i].expected_ec_bmask, retval);
	}
}

/**
 * @brief TestPurpose: Verify EC to Zephyr GPIO bitmask conversion.
 *
 * @details
 * Validate EC to Zephyr GPIO bitmask conversion.
 *
 * Expected Results
 *  - GPIO bitmask conversions are successful
 */
ZTEST(gpio, test_convert_to_zephyr_flags)
{
	gpio_flags_t retval;

	struct {
		gpio_flags_t ec_bmask;
		int expected_zephyr_bmask;
	} validate[] = {
		{ GPIO_FLAG_NONE, GPIO_DISCONNECTED },
		{ GPIO_LOW, GPIO_OUTPUT_INIT_LOW },
		{ GPIO_HIGH, GPIO_OUTPUT_INIT_HIGH },
		{ GPIO_INT_F_RISING,
		  GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_HIGH_1 },
		{ GPIO_INT_F_FALLING,
		  GPIO_INT_ENABLE | GPIO_INT_EDGE | GPIO_INT_LOW_0 },
		{ GPIO_INT_F_LOW, GPIO_INT_ENABLE | GPIO_INT_LOW_0 },
		{ GPIO_INT_F_HIGH, GPIO_INT_ENABLE | GPIO_INT_HIGH_1 },
		{ GPIO_SEL_1P8V, GPIO_VOLTAGE_1P8 },
		{ GPIO_LOCKED, 0 },
	};
	int num_tests = ARRAY_SIZE(validate);

	for (int i = 0; i < num_tests; i++) {
		retval = convert_to_zephyr_flags(validate[i].ec_bmask);
		zassert_equal(validate[i].expected_zephyr_bmask, retval,
			      "Expected 0x%08X, returned 0x%08X.",
			      validate[i].expected_zephyr_bmask, retval);
	}
}

/**
 * @brief TestPurpose: Verify GPIO signal_is_gpio.
 *
 * @details
 * Validate signal_is_gpio
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_signal_is_gpio)
{
	zassert_equal(true, signal_is_gpio(GPIO_TEST), "Expected true");
}

/**
 * @brief TestPurpose: Verify GPIO set/get level.
 *
 * @details
 * Validate set/get level
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_gpio_get_set_level)
{
	enum gpio_signal signal = GPIO_TEST;
	int revert_level = gpio_get_level(signal);
	int level;

	/* Test invalid signal */
	gpio_set_level(GPIO_COUNT, 0);
	zassert_equal(0, gpio_get_level(GPIO_COUNT), "Expected level==0");

	/* Test valid signal */
	gpio_set_level(signal, 0);
	zassert_equal(0, gpio_get_level(signal), "Expected level==0");

	gpio_set_level(signal, 1);
	zassert_equal(1, gpio_get_level(signal), "Expected level==1");

	level = gpio_get_ternary(signal);
	zassert_equal(2, level, "Expected level==2, returned=%d", level);

	gpio_set_level_verbose(CC_CHIPSET, signal, 0);

	gpio_set_level(signal, revert_level);
}

/**
 * @brief TestPurpose: Verify GPIO get name.
 *
 * @details
 * Validate gpio_get_name
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_gpio_get_name)
{
	enum gpio_signal signal = GPIO_TEST;
	const char *signal_name;

	/* Test invalid signal */
	signal_name = gpio_get_name(GPIO_COUNT);
	zassert_true(!strcasecmp("UNIMPLEMENTED", signal_name),
		     "gpio_get_name returned a valid signal \'%s\'",
		     signal_name);

	/* Test valid signal */
	signal_name = gpio_get_name(signal);
	zassert_true(!strcasecmp("TEST", signal_name),
		     "gpio_get_name returned a valid signal \'%s\'",
		     signal_name);
}

/**
 * @brief TestPurpose: Verify GPIO get default flags.
 *
 * @details
 * Validate gpio_get_default_flags
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_gpio_get_default_flags)
{
	enum gpio_signal signal = GPIO_TEST;
	int retval;

	/* Test invalid signal */
	retval = gpio_get_default_flags(GPIO_COUNT);
	zassert_equal(0, retval, "Expected 0x0, returned 0x%08X", retval);
	gpio_set_flags(GPIO_COUNT, GPIO_INPUT);

	/* Test valid signal */
	retval = gpio_get_default_flags(signal);
	zassert_equal(GPIO_INPUT | GPIO_OUTPUT, retval,
		      "Expected 0x%08x, returned 0x%08X",
		      GPIO_INPUT | GPIO_OUTPUT, retval);

	gpio_set_flags(signal, GPIO_INPUT | GPIO_OUTPUT);
}

/**
 * @brief TestPurpose: Verify GPIO reset.
 *
 * @details
 * Validate gpio_reset
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_gpio_reset)
{
	enum gpio_signal signal = GPIO_TEST;

	gpio_reset(GPIO_COUNT);
	gpio_reset(signal);
}

/**
 * @brief TestPurpose: Verify GPIO enable/disable interrupt.
 *
 * @details
 * Validate gpio_enable_interrupt
 *
 * Expected Results
 *  - Success
 */
ZTEST(gpio, test_gpio_enable_interrupt)
{
	/* Test invalid signal */
	zassert_equal(-1, gpio_disable_interrupt(GPIO_COUNT),
		      "Expected failure");
	zassert_equal(-1, gpio_enable_interrupt(GPIO_COUNT),
		      "Expected failure");

	/* Test valid signal */
	zassert_equal(0, gpio_disable_interrupt(GPIO_TEST), "Expected success");
	zassert_equal(0, gpio_enable_interrupt(GPIO_TEST), "Expected success");
}

/**
 * @brief GPIO test setup handler.
 */
static void gpio_before(void *state)
{
	ARG_UNUSED(state);
	set_test_runner_tid();
}

/**
 * @brief GPIO test teardown handler.
 */
static void gpio_after(void *state)
{
	ARG_UNUSED(state);
}

/**
 * @brief Test Suite: Verifies GPIO functionality.
 */
ZTEST_SUITE(gpio, drivers_predicate_post_main, NULL, gpio_before, gpio_after,
	    NULL);
