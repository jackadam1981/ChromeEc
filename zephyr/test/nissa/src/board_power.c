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

#include <ap_power_override_functions.h>
#include <common.h>
#include <power_signals.h>

FAKE_VALUE_FUNC(int, extpower_is_present);

int real_board_power_signal_get(enum power_signal);
FAKE_VALUE_FUNC(int, board_power_signal_get, enum power_signal);
int real_board_power_signal_set(enum power_signal, int value);
FAKE_VALUE_FUNC(int, board_power_signal_set, enum power_signal, int);

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
	RESET_FAKE(board_power_signal_get);
	RESET_FAKE(board_power_signal_set);
}

ZTEST_SUITE(nissa_board_power, NULL, suite_setup, before_test, NULL, NULL);

ZTEST(nissa_board_power, test_power_signal_set_is_a_nop)
{
	zassert_equal(real_board_power_signal_set(PWR_EN_PP3300_A, 1), -EINVAL);
}

ZTEST(nissa_board_power, test_power_signal_get)
{
	const struct gpio_dt_spec *all_sys_pwrgd_in =
		GPIO_DT_FROM_NODELABEL(gpio_all_sys_pwrgd);

	/*
	 * ALL_SYS_PWRGD is asserted when SLP_S3 is deasserted, the
	 * corresponding GPIO is asserted, and PG_PP1P05 is asserted.
	 */
	board_power_signal_get_fake.return_val_seq = (int[]){ 0, 1 };
	board_power_signal_get_fake.return_val_seq_len = 2;
	zassert_ok(gpio_emul_input_set(all_sys_pwrgd_in->port,
				       all_sys_pwrgd_in->pin, 1));

	zassert_true(real_board_power_signal_get(PWR_ALL_SYS_PWRGD));
	zassert_equal(board_power_signal_get_fake.call_count, 2);
	zassert_equal(board_power_signal_get_fake.arg0_history[0], PWR_SLP_S3);
	zassert_equal(board_power_signal_get_fake.arg0_history[1],
		      PWR_PG_PP1P05);

	board_power_signal_get_fake.return_val_seq = (int[]){ 0, 0 };
	board_power_signal_get_fake.return_val_seq_idx = 0;
	zassert_false(real_board_power_signal_get(PWR_ALL_SYS_PWRGD));

	board_power_signal_get_fake.return_val_seq_idx = 0;
	zassert_ok(gpio_emul_input_set(all_sys_pwrgd_in->port,
				       all_sys_pwrgd_in->pin, 0));
	zassert_false(real_board_power_signal_get(PWR_ALL_SYS_PWRGD));

	board_power_signal_get_fake.return_val = 1;
	board_power_signal_get_fake.return_val_seq_len = 0;
	zassert_false(real_board_power_signal_get(PWR_ALL_SYS_PWRGD));

	/* Other signals are invalid */
	zassert_equal(real_board_power_signal_get(PWR_EN_PP3300_A), -EINVAL);
}

static int fake_get_signal_dsw_pwrok_asserted(enum power_signal signal)
{
	return signal == PWR_DSW_PWROK;
}

ZTEST(nissa_board_power, test_g3_s5_action)
{
	/*
	 * DSW_PWROK (PP3300_A power good) is asserted, to be copied to
	 * DSW_PWROK output to SoC. This uses power_wait_signals internally
	 * and may call power_signal_get() many times, so we use a custom fake
	 * rather than specifying a sequence.
	 */
	board_power_signal_get_fake.custom_fake =
		fake_get_signal_dsw_pwrok_asserted;

	board_ap_power_action_g3_s5();
	/* Rails were turned on, and DSW_PWROK to SoC asserted */
	zassert_equal(board_power_signal_set_fake.call_count, 3,
		      "actual call count was %d",
		      board_power_signal_set_fake.call_count);
	zassert_equal(board_power_signal_set_fake.arg0_history[0],
		      PWR_EN_PP5000_A);
	zassert_true(board_power_signal_set_fake.arg1_history[0]);
	zassert_equal(board_power_signal_set_fake.arg0_history[1],
		      PWR_EN_PP3300_A);
	zassert_true(board_power_signal_set_fake.arg1_history[1]);
	zassert_equal(board_power_signal_set_fake.arg0_history[2],
		      PWR_EC_SOC_DSW_PWROK);
	zassert_true(board_power_signal_set_fake.arg1_history[2]);
}

ZTEST(nissa_board_power, test_rails_enabled)
{
	board_power_signal_get_fake.return_val = true;
	zassert_true(board_ap_power_check_power_rails_enabled());
	zassert_equal(board_power_signal_get_fake.call_count, 3);
	zassert_equal(board_power_signal_get_fake.arg0_history[0],
		      PWR_EN_PP3300_A);
	zassert_equal(board_power_signal_get_fake.arg0_history[1],
		      PWR_EN_PP5000_A);
	zassert_equal(board_power_signal_get_fake.arg0_history[2],
		      PWR_EC_SOC_DSW_PWROK);

	board_power_signal_get_fake.return_val = false;
	zassert_false(board_ap_power_check_power_rails_enabled());
	zassert_equal(board_power_signal_get_fake.arg0_val, PWR_EN_PP3300_A);
}