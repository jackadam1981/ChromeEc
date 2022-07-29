/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fff.h>
#include <zephyr/ztest_assert.h>
#include <ztest.h>

#include "ec_commands.h"
#include "led.h"
#include "led_common.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

ZTEST_SUITE(pwm_led_driver, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);

FAKE_VOID_FUNC2(led_set_color, enum led_color, enum ec_led_id);

ZTEST(pwm_led_driver, test_led_set_brightness)
{
	uint8_t brightness[EC_LED_COLOR_COUNT] = {};

	RESET_FAKE(led_set_color);

	/* Zero array, call led_set_color(LED_OFF, LEFT_LED) */
	led_set_brightness(EC_LED_ID_LEFT_LED, brightness);

	brightness[EC_LED_COLOR_AMBER] = 1;
	/* Unsupported color, call led_set_color(LED_OFF, LEFT_LED) */
	led_set_brightness(EC_LED_ID_LEFT_LED, brightness);
	/* Supported, call led_set_color(AMBER, RIGHT_LED) */
	led_set_brightness(EC_LED_ID_RIGHT_LED, brightness);

	/* verify the call history */
	zassert_equal(led_set_color_fake.call_count, 3, NULL);

	zassert_equal(led_set_color_fake.arg0_history[0], LED_OFF, NULL);
	zassert_equal(led_set_color_fake.arg1_history[0], EC_LED_ID_LEFT_LED,
		      NULL);

	zassert_equal(led_set_color_fake.arg0_history[1], LED_OFF, NULL);
	zassert_equal(led_set_color_fake.arg1_history[1], EC_LED_ID_LEFT_LED,
		      NULL);

	zassert_equal(led_set_color_fake.arg0_history[2], LED_AMBER, NULL);
	zassert_equal(led_set_color_fake.arg1_history[2], EC_LED_ID_RIGHT_LED,
		      NULL);
}

ZTEST(pwm_led_driver, test_led_get_brightness)
{
	uint8_t brightness[EC_LED_COLOR_COUNT];
	const uint8_t expected_left[EC_LED_COLOR_COUNT] = {
		[EC_LED_COLOR_BLUE] = 100,
		[EC_LED_COLOR_WHITE] = 100,
	};
	const uint8_t expected_right[EC_LED_COLOR_COUNT] = {
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
