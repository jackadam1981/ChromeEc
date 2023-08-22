/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests for program/nissa/src/board_power.c.
 */
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/include/emul/emul_power_signals.h>
#include <zephyr/ztest.h>

void board_ap_power_action_g3_s5(void);

FAKE_VALUE_FUNC(int, extpower_is_present);

static void *suite_setup(void)
{
	// This should probably not be done, since we're pretending to be the
	// sequencer and actually running the real one is complex.
	// ap_pwrseq_start(???)
	return NULL;
}

static void before_test(void *fixture)
{
	RESET_FAKE(extpower_is_present);
}

ZTEST_SUITE(nissa_board_power, NULL, suite_setup, before_test, NULL, NULL);

ZTEST(nissa_board_power, test_g3_s5_action)
{
	zassert_ok(power_signal_emul_load(
		EMUL_POWER_SIGNAL_TEST_PLATFORM(sequence_g3_to_s5)));
	// board_ap_power_action_g3_s5();
}
