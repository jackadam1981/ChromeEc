/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <cros_board_info.h>
#include <cros_cbi.h>
#include <fan.h>
#include <hooks.h>

FAKE_VOID_FUNC(fan_set_duty, int, int);
FAKE_VALUE_FUNC(int, fan_get_duty, int);

struct fan_conf conf;
struct fan_rpm rpm;

const struct fan_t fans[1] = { { .conf = &conf, .rpm = &rpm } };
struct fan_data fan_data[1];

K_TIMER_DEFINE(ktimer, NULL, NULL);

static void fan_set_duty_mock(int ch, int duty)
{
	zassert_equal(ch, 0);
	zassert_between_inclusive(duty, 0, 100);

	fan_data[ch].pwm_percent = duty;
}

static int fan_get_duty_mock(int ch)
{
	zassert_equal(ch, 0);
	zassert_between_inclusive(fan_data[ch].pwm_percent, 0, 100);

	return fan_data[ch].pwm_percent;
}

static int duty_to_rpm(int duty)
{
	const float ratio = 0.015;

	zassert_between_inclusive(duty, 0, 100);

	return (int)((float)duty / ratio);
}

static void fan_tick(void)
{
	int rpm_diff;
	int duty;

	duty = fan_data[0].pwm_percent;

	rpm_diff = duty_to_rpm(duty) - fan_data[0].rpm_actual;

	/* Clamp rpm_diff. This essentially emulates fan inertia. */
	if (rpm_diff > 0)
		rpm_diff = MIN(rpm_diff, 500);
	if (rpm_diff < 0)
		rpm_diff = MAX(rpm_diff, -500);

	fan_data[0].rpm_actual += rpm_diff;
}

static void fan_test_begin(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(fan_set_duty);
	RESET_FAKE(fan_get_duty);

	fan_set_duty_fake.custom_fake = fan_set_duty_mock;
	fan_get_duty_fake.custom_fake = fan_get_duty_mock;

	/*
	 * This is normally read from DT.
	 * The problem is that we don't want to pull the entire fan framework
	 * for this test. Instead initialize it here.
	 */
	memset(&conf, 0, sizeof(conf));
	memset(&rpm, 0, sizeof(rpm));

	/* We only need to set one parameter here. */
	rpm.rpm_deviation = 3;

	memset(&fan_data[0], 0, sizeof(fan_data[0]));
}

/* Only FAN 0 should be supported. */
ZTEST(fan, test_fan_invalid_arg)
{
	enum fan_status status;

	status = board_override_fan_control_duty(1);
	zassert_equal(status, FAN_STATUS_FRUSTRATED);
}

#define FAN_MAX_RPM 4800

/* Check whether we can ramp up into 4800 in 5s */
ZTEST(fan, test_fan_max_rpm)
{
	enum fan_status status;
	int deviation;

	fan_data[0].rpm_target = FAN_MAX_RPM;
	k_timer_start(&ktimer, K_SECONDS(5), K_NO_WAIT);

	while (k_timer_remaining_ticks(&ktimer) != 0) {
		status = board_override_fan_control_duty(0);
		zassert_not_equal(status, FAN_STATUS_FRUSTRATED);
		fan_tick();
		k_sleep(K_TICKS(1));
	}

	deviation = rpm.rpm_deviation * FAN_MAX_RPM / 100;
	zassert_within(fan_data[0].rpm_actual, FAN_MAX_RPM, deviation);
}

/* Check for FAN_STATUS_STOPPED when the fan is in fact stopped. */
ZTEST(fan, test_fan_off)
{
	enum fan_status status;

	fan_data[0].rpm_target = 0;

	status = board_override_fan_control_duty(0);
	zassert_equal(status, FAN_STATUS_STOPPED);
}

/* If we can't achieve selected RPM, FAN_STATUS_FRUSTRATED is expected. */
ZTEST(fan, test_fan_frustrated_max)
{
	enum fan_status status;
	int i;

	fan_data[0].rpm_target = 10000;

	/*
	 * 600 ticks should be more than enough for implementation to realize
	 * that it can't get up to 10k RPM
	 */
	for (i = 0; i < 600; i++) {
		status = board_override_fan_control_duty(0);
		if (status == FAN_STATUS_FRUSTRATED)
			break;

		fan_tick();
		k_sleep(K_TICKS(1));
	}
	zassert_equal(status, FAN_STATUS_FRUSTRATED);
}

ZTEST_SUITE(fan, NULL, NULL, fan_test_begin, NULL, NULL);
