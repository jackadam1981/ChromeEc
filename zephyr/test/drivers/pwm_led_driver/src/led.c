/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fff.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/ztest_assert.h>
#include <ztest.h>

#include "ec_commands.h"
#include "gpio.h"
#include "include/power.h"
#include "led.h"
#include "led_common.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

/*
#define VERIFY_LED_COLOR(color, led_id)                                    \
	{                                                                  \
		const struct led_pins_node_t *pin_node =                   \
			led_get_node(color, led_id);                       \
		for (int j = 0; j < pin_node->pins_count; j++) {           \
			int val = gpio_pin_get_dt(gpio_get_dt_spec(        \
				pin_node->pwm_pins[j].signal));           \
			int expecting = pin_node->pwm_pins[j].val;        \
			zassert_equal(expecting, val, "[%d]: %d != %d", j, \
				      expecting, val);                     \
		}                                                          \
	}
*/

#define VERIFY_LED_COLOR(color, led_id)

ZTEST_SUITE(led_driver, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(led_driver, test_led_brightness)
{
	uint8_t brightness[EC_LED_COLOR_COUNT];
	uint8_t expected_left[EC_LED_COLOR_COUNT] = {
		[EC_LED_COLOR_BLUE] = 100,
		[EC_LED_COLOR_WHITE] = 100,
	};
	uint8_t expected_right[EC_LED_COLOR_COUNT] = {
		[EC_LED_COLOR_WHITE] = 100,
		[EC_LED_COLOR_AMBER] = 100,
	};

	/* Verify LED colors defined in device tree are reflected in the
	 * brightness array.
	 */
	memset(brightness, 255, sizeof(brightness));
	led_get_brightness_range(EC_LED_ID_LEFT_LED, brightness);
	zassert_mem_equal(brightness, expected_left, sizeof(brightness), NULL);

	memset(brightness, 255, sizeof(brightness));
	led_get_brightness_range(EC_LED_ID_RIGHT_LED, brightness);
	zassert_mem_equal(brightness, expected_right, sizeof(brightness), NULL);
}
