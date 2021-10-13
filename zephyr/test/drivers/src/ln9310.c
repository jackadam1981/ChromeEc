/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>
#include <drivers/emul.h>
#include <kernel.h>
#include <ztest_assert.h>
#include <drivers/i2c_emul.h>

#include "driver/ln9310.h"
#include "emul/emul_ln9310.h"
#include "emul/emul_common_i2c.h"
#include "timer.h"

/*
 * TODO(b/201420132): Implement approach for tests to immediately schedule work
 * to avoid any sleeping
 */
#define TEST_DELAY_MS 50

/*
 * Chip revisions below LN9310_BC_STS_C_CHIP_REV_FIXED require an alternative
 * software startup to properly initialize and power up.
 */
#define REQUIRES_CFLY_PRECHARGE_STARTUP_CHIP_REV \
	(LN9310_BC_STS_C_CHIP_REV_FIXED - 1)

static void test_ln9310_read_chip_fails(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery and chip rev won't matter here so only testing one pair */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	i2c_common_emul_set_read_fail_reg(i2c_emul, LN9310_REG_BC_STS_C);

	zassert_ok(!ln9310_init(), NULL);
	zassert_false(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
}

static void test_ln9310_2s_powers_up(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	ln9310_software_enable(1);

	k_msleep(TEST_DELAY_MS);
	zassert_true(ln9310_power_good(), NULL);
}

static void test_ln9310_3s_powers_up(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_3S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	ln9310_software_enable(1);

	k_msleep(TEST_DELAY_MS);
	zassert_true(ln9310_power_good(), NULL);
}

struct startup_workaround_data {
	bool startup_workaround_attempted;
	bool startup_workaround_should_fail;
};

static int mock_write_fn_intercept_startup_workaround(struct i2c_emul *emul,
						      int reg, uint8_t val,
						      int bytes, void *data)
{
	struct startup_workaround_data *test_data = data;

	uint8_t startup_workaround_val =
		(LN9310_TEST_MODE_CTRL_FORCE_SC_OUT_PRECHARGE_ON |
		 LN9310_TEST_MODE_CTRL_FORCE_SC_OUT_PREDISCHARGE_ON);

	test_data->startup_workaround_attempted =
		test_data->startup_workaround_attempted ||
		((reg == LN9310_REG_TEST_MODE_CTRL) &&
		 (val == startup_workaround_val));

	if (test_data->startup_workaround_should_fail)
		return -1;

	return 1;
}

static void test_ln9310_2s_cfly_precharge_startup(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));

	struct i2c_emul *emul = ln9310_emul_get_i2c_emul(emulator);

	struct startup_workaround_data test_data = {
		.startup_workaround_attempted = false,
		.startup_workaround_should_fail = false,
	};

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator,
				REQUIRES_CFLY_PRECHARGE_STARTUP_CHIP_REV);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_write_func(
		emul, &mock_write_fn_intercept_startup_workaround, &test_data);

	ln9310_software_enable(1);
	zassert_true(test_data.startup_workaround_attempted, NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_true(ln9310_power_good(), NULL);

	ln9310_software_enable(0);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
}

static void test_ln9310_3s_cfly_precharge_startup(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *emul = ln9310_emul_get_i2c_emul(emulator);

	struct startup_workaround_data test_data = {
		.startup_workaround_attempted = false,
		.startup_workaround_should_fail = false,
	};

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_3S);
	ln9310_emul_set_version(emulator,
				REQUIRES_CFLY_PRECHARGE_STARTUP_CHIP_REV);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_write_func(
		emul, &mock_write_fn_intercept_startup_workaround, &test_data);

	ln9310_software_enable(1);
	zassert_true(test_data.startup_workaround_attempted, NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_true(ln9310_power_good(), NULL);

	ln9310_software_enable(0);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
}

static void test_ln9310_cfly_precharge_exceeds_retries(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));

	struct i2c_emul *emul = ln9310_emul_get_i2c_emul(emulator);

	struct startup_workaround_data test_data = {
		.startup_workaround_attempted = false,
		.startup_workaround_should_fail = true,
	};

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/*
	 * Battery and chip rev won't matter for statement
	 * coverage here so only testing one pair.
	 */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator,
				REQUIRES_CFLY_PRECHARGE_STARTUP_CHIP_REV);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_write_func(
		emul, &mock_write_fn_intercept_startup_workaround, &test_data);

	ln9310_software_enable(1);
	zassert_true(test_data.startup_workaround_attempted, NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
}

static void test_ln9310_battery_unknown(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));

	zassert_not_null(emulator, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/*
	 * Chip rev won't matter for statement
	 * cov so only testing one version.
	 */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_UNKNOWN);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	zassert_ok(!ln9310_init(), NULL);
	zassert_false(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	ln9310_software_enable(1);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
}

static void test_ln9310_2s_battery_read_fails(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery won't matter here so only testing one version */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	i2c_common_emul_set_read_fail_reg(i2c_emul, LN9310_REG_BC_STS_B);

	zassert_ok(!ln9310_init(), NULL);
	zassert_false(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
}

static void test_ln9310_lion_ctrl_reg_fails(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery won't matter here so only testing one version */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	i2c_common_emul_set_read_fail_reg(i2c_emul, LN9310_REG_LION_CTRL);

	zassert_ok(!ln9310_init(), NULL);
	zassert_false(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	ln9310_software_enable(1);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
}


/* Needed so we can mock time after fn stack frame popped */
static timestamp_t ln9310_mock_time;

static int mock_intercept_startup_ctrl_reg(struct i2c_emul *emul, int reg,
					   uint8_t val, int bytes, void *data)
{
	bool *handled_clearing_standby_en_bit_timeout = data;

	if (reg == LN9310_REG_STARTUP_CTRL &&
	    !(*handled_clearing_standby_en_bit_timeout)) {
		if (val == 0) {
			timestamp_t time = get_time();

			time.val += 1 + LN9310_CFLY_PRECHARGE_TIMEOUT;
			ln9310_mock_time = time;
			get_time_mock = &ln9310_mock_time;
		} else {
			/* ln9310 aborts a startup attempt */
			*handled_clearing_standby_en_bit_timeout = true;
			get_time_mock = NULL;
		}
	}
	return 1;
}

static void test_ln9310_cfly_precharge_timesout(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);
	bool handled_clearing_standby_en_bit_timeout = false;

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery and chip rev won't matter here so only testing one pair */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator,
				REQUIRES_CFLY_PRECHARGE_STARTUP_CHIP_REV);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);

	i2c_common_emul_set_write_func(
		i2c_emul, &mock_intercept_startup_ctrl_reg,
		&handled_clearing_standby_en_bit_timeout);

	ln9310_software_enable(1);
	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_true(handled_clearing_standby_en_bit_timeout, NULL);
	/* It only times out on one attempt, it should subsequently startup */
	zassert_true(ln9310_power_good(), NULL);
}

static int reg_access_to_fail;
static int reg_access_fail_countdown;
static int mock_read_intercept_reg_to_fail(struct i2c_emul *emul, int reg,
					   uint8_t *val, int bytes, void *data)
{
	if (reg == reg_access_to_fail) {
		reg_access_fail_countdown--;
		if (reg_access_fail_countdown <= 0)
			return -1;
	}
	return 1;
}

static void test_ln9310_interrupt_reg_fail(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery and chip rev won't matter here so only testing one pair */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	i2c_common_emul_set_read_func(i2c_emul,
				      &mock_read_intercept_reg_to_fail, NULL);

	/* Fail in beginning of software enable */
	reg_access_to_fail = LN9310_REG_INT1;
	reg_access_fail_countdown = 1;

	ln9310_software_enable(1);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
	zassert_true(reg_access_fail_countdown <= 0, NULL);

	/* Fail in irq interrupt handler */
	reg_access_fail_countdown = 2;

	ln9310_software_enable(1);
	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);
	zassert_false(ln9310_power_good(), NULL);
	zassert_true(reg_access_fail_countdown <= 0, NULL);

	i2c_common_emul_set_read_func(i2c_emul, NULL, NULL);
}

static void test_ln9310_sys_sts_reg_fail(void)
{
	const struct emul *emulator =
		emul_get_binding(DT_LABEL(DT_NODELABEL(ln9310)));
	struct i2c_emul *i2c_emul = ln9310_emul_get_i2c_emul(emulator);

	zassert_not_null(emulator, NULL);
	zassert_not_null(i2c_emul, NULL);

	ln9310_emul_set_context(emulator);
	ln9310_emul_reset(emulator);
	/* Battery and chip rev won't matter here so only testing one pair */
	ln9310_emul_set_battery_cell_type(emulator, BATTERY_CELL_TYPE_2S);
	ln9310_emul_set_version(emulator, LN9310_BC_STS_C_CHIP_REV_FIXED);

	zassert_ok(ln9310_init(), NULL);
	zassert_true(ln9310_emul_is_init(emulator), NULL);

	i2c_common_emul_set_read_func(i2c_emul,
				      &mock_read_intercept_reg_to_fail, NULL);

	/* Register only read once and in the interrupt handler */
	reg_access_to_fail = LN9310_REG_SYS_STS;
	reg_access_fail_countdown = 1;

	ln9310_software_enable(1);

	/* TODO(b/201420132) */
	k_msleep(TEST_DELAY_MS);

	zassert_false(ln9310_power_good(), NULL);
	zassert_true(reg_access_fail_countdown <= 0, NULL);

	i2c_common_emul_set_read_func(i2c_emul, NULL, NULL);
}

static void reset_ln9310_state(void)
{
	ln9310_reset_to_initial_state();
	get_time_mock = NULL;
	reg_access_to_fail = 0;
	reg_access_fail_countdown = 0;
}

void test_suite_ln9310(void)
{
	ztest_test_suite(
		ln9310,
		ztest_unit_test_setup_teardown(
			test_ln9310_sys_sts_reg_fail,
			reset_ln9310_state,
			reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_interrupt_reg_fail,
			reset_ln9310_state,
			reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_cfly_precharge_timesout,
			reset_ln9310_state,
			reset_ln9310_state),
		ztest_unit_test_setup_teardown(test_ln9310_lion_ctrl_reg_fails,
					       reset_ln9310_state,
					       reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_2s_battery_read_fails,
			reset_ln9310_state,
			reset_ln9310_state),
		ztest_unit_test_setup_teardown(test_ln9310_battery_unknown,
					       reset_ln9310_state,
					       reset_ln9310_state),
		ztest_unit_test_setup_teardown(test_ln9310_read_chip_fails,
					       reset_ln9310_state,
					       reset_ln9310_state),
		ztest_unit_test_setup_teardown(test_ln9310_2s_powers_up,
					       reset_ln9310_state,
					       reset_ln9310_state),
		ztest_unit_test_setup_teardown(test_ln9310_3s_powers_up,
					       reset_ln9310_state,
					       reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_cfly_precharge_exceeds_retries,
			reset_ln9310_state, reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_2s_cfly_precharge_startup,
			reset_ln9310_state, reset_ln9310_state),
		ztest_unit_test_setup_teardown(
			test_ln9310_3s_cfly_precharge_startup,
			reset_ln9310_state, reset_ln9310_state));
	ztest_run_test_suite(ln9310);
}
