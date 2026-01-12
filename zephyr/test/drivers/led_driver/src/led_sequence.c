/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/ztest.h>

void set_board_led_alt_policy(int label);

static void *led_sequence_setup(void)
{
	/* Sleep to allow init functions to complete and timing to settle */
	k_sleep(K_MSEC(1000));

	return NULL;
}

ZTEST_SUITE(led_driver_sequence, drivers_predicate_post_main,
	    led_sequence_setup, NULL, NULL, NULL);

static bool is_blue_on(void)
{
	return gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_chg_led_y_c0));
}

static bool is_white_on(void)
{
	return gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_chg_led_w_c0));
}

static void wait_and_assert_led_state(bool (*check_fn)(void), int min_ms,
				      int max_ms, const char *state_name)
{
	int64_t start = k_uptime_get();
	int64_t now;
	int64_t elapsed;

	while (true) {
		if (check_fn()) {
			break;
		}
		now = k_uptime_get();
		elapsed = now - start;
		zassert_true(
			elapsed <= max_ms,
			"Timeout waiting for %s. Elapsed: %lld ms, Limit: %d ms",
			state_name, elapsed, max_ms);
		k_sleep(K_MSEC(10));
	}

	now = k_uptime_get();
	elapsed = now - start;
	zassert_true(
		elapsed >= min_ms,
		"Transition to %s too fast. Elapsed: %lld ms, Expected >= %d ms",
		state_name, elapsed, min_ms);
}

ZTEST(led_driver_sequence, test_infinite_loop)
{
	/* Select the infinite loop policy node */
	set_board_led_alt_policy(2);
	led_control(EC_LED_ID_BATTERY_LED, LED_STATE_RESET);
	hook_notify(HOOK_TICK);
	zassert_true(is_blue_on(), "Sync to Blue should occur immediately");

	/* Pattern: Blue(250ms) -> White(250ms). Test 5 full cycles. */
	for (int i = 0; i < 5; i++) {
		/*
		 * Lower bound: 1 animation tick (30ms) early + polling margin.
		 * Upper bound: 1 animation tick (30ms) late + polling margin.
		 */
		wait_and_assert_led_state(is_white_on, 200, 300, "White");
		wait_and_assert_led_state(is_blue_on, 200, 300, "Blue");
	}
}

ZTEST(led_driver_sequence, test_run_once)
{
	/* Select the run-once policy node */
	set_board_led_alt_policy(3);
	led_control(EC_LED_ID_BATTERY_LED, LED_STATE_RESET);

	/* At 0ms: White */
	hook_notify(HOOK_TICK);
	zassert_true(is_white_on());

	/* At 250ms: Blue */
	wait_and_assert_led_state(is_blue_on, 200, 300, "Blue");

	/*
	 * Pattern cycle limit (1) has been reached. Test several more ticks to
	 * verify the state remains held on Blue.
	 */
	for (int i = 0; i < 10; i++) {
		k_sleep(K_MSEC(250));
		zassert_true(is_blue_on(), "Tick %d: Expected to hold Blue", i);
	}
}

ZTEST(led_driver_sequence, test_instant_init)
{
	/* Select the instant transition at start policy node */
	set_board_led_alt_policy(4);
	led_control(EC_LED_ID_BATTERY_LED, LED_STATE_RESET);

	hook_notify(HOOK_TICK);
	zassert_true(is_white_on(),
		     "Failed to skip initial 0ms steps to White");
}

ZTEST(led_driver_sequence, test_all_zero_safety_guard)
{
	/* Select the all-zero policy node */
	set_board_led_alt_policy(5);
	led_control(EC_LED_ID_BATTERY_LED, LED_STATE_RESET);

	/* Safety guard should prevent infinite loop and CPU hang */
	hook_notify(HOOK_TICK);
	zassert_true(is_white_on() || is_blue_on());
}

ZTEST(led_driver_sequence, test_mid_pattern_instant)
{
	/* Select the instant transition in middle policy node */
	set_board_led_alt_policy(6);
	led_control(EC_LED_ID_BATTERY_LED, LED_STATE_RESET);

	/* At 0ms: Land on Blue */
	hook_notify(HOOK_TICK);
	zassert_true(is_blue_on());

	/* At 250ms: Blue expires, skips Off(0ms), lands on White */
	wait_and_assert_led_state(is_white_on, 200, 300, "White");
}
