/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>
#include <stdint.h>

#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "pwm.h"
#include "task.h"
#include "timer.h"

#define PWM_CH_COGO	PWM_CH_KBLIGHT

#define COGO_TICK_INTERVAL_US (50 * MSEC)	/* 20 Hz */

/*
 * one cosine cycle, 100% to 20%
 * in 80 steps (4 seconds at 20 Hz)
 */

static const uint8_t v_cosine[] = {
	[  0] = 100, [  1] =  99, [  2] =  99, [  3] =  98,
	[  4] =  98, [  5] =  96, [  6] =  95, [  7] =  94,
	[  8] =  92, [  9] =  90, [ 10] =  88, [ 11] =  85,
	[ 12] =  83, [ 13] =  80, [ 14] =  78, [ 15] =  75,
	[ 16] =  72, [ 17] =  69, [ 18] =  66, [ 19] =  63,
	[ 20] =  60, [ 21] =  56, [ 22] =  53, [ 23] =  50,
	[ 24] =  47, [ 25] =  44, [ 26] =  41, [ 27] =  39,
	[ 28] =  36, [ 29] =  34, [ 30] =  31, [ 31] =  29,
	[ 32] =  27, [ 33] =  25, [ 34] =  24, [ 35] =  23,
	[ 36] =  21, [ 37] =  21, [ 38] =  20, [ 39] =  20,
	[ 40] =  20, [ 41] =  20, [ 42] =  20, [ 43] =  21,
	[ 44] =  21, [ 45] =  23, [ 46] =  24, [ 47] =  25,
	[ 48] =  27, [ 49] =  29, [ 50] =  31, [ 51] =  34,
	[ 52] =  36, [ 53] =  39, [ 54] =  41, [ 55] =  44,
	[ 56] =  47, [ 57] =  50, [ 58] =  53, [ 59] =  56,
	[ 60] =  59, [ 61] =  63, [ 62] =  66, [ 63] =  69,
	[ 64] =  72, [ 65] =  75, [ 66] =  78, [ 67] =  80,
	[ 68] =  83, [ 69] =  85, [ 70] =  88, [ 71] =  90,
	[ 72] =  92, [ 73] =  94, [ 74] =  95, [ 75] =  96,
	[ 76] =  98, [ 77] =  98, [ 78] =  99, [ 79] =  99,
};

/*
 * ramp up from 0% to 100% (half cosine + 180deg)
 * in 40 steps (2 seconds at 20 Hz)
 */

static const uint8_t v_ramp_up[] = {
	[  0] =   0, [  1] =   0, [  2] =   0, [  3] =   1,
	[  4] =   2, [  5] =   3, [  6] =   5, [  7] =   7,
	[  8] =   9, [  9] =  11, [ 10] =  14, [ 11] =  17,
	[ 12] =  20, [ 13] =  23, [ 14] =  27, [ 15] =  30,
	[ 16] =  34, [ 17] =  38, [ 18] =  42, [ 19] =  46,
	[ 20] =  49, [ 21] =  53, [ 22] =  57, [ 23] =  61,
	[ 24] =  65, [ 25] =  69, [ 26] =  72, [ 27] =  76,
	[ 28] =  79, [ 29] =  82, [ 30] =  85, [ 31] =  88,
	[ 32] =  90, [ 33] =  92, [ 34] =  94, [ 35] =  96,
	[ 36] =  97, [ 37] =  98, [ 38] =  99, [ 39] =  99,
};

void cogo_task(void *u)
{
	const uint8_t *anim_data;
	unsigned int anim_points;
	unsigned int step;
	timestamp_t ts_now = get_time();
	timestamp_t ts_next = { .val = 0 };

	pwm_set_duty(PWM_CH_COGO, 0);
	pwm_enable(PWM_CH_COGO, 1);
	gpio_set_level(GPIO_EC_KB_BL_EN_L, 0);

	/* ramp up signal */

	anim_data = v_ramp_up;
	anim_points = ARRAY_SIZE(v_ramp_up);

	step = 0;
	do {
		ts_now = get_time();
		if (ts_now.val < ts_next.val) {
			usleep(ts_next.val - ts_now.val);
			continue;
		}
		ts_next.val = ts_now.val + COGO_TICK_INTERVAL_US;

		pwm_set_duty(PWM_CH_COGO, anim_data[step]);
		step = (step + 1) % anim_points;
	} while (step != 0);

	/* output wave forever */

	anim_data = v_cosine;
	anim_points = ARRAY_SIZE(v_cosine);

	step = 0;
	while (1) {
		ts_now = get_time();
		if (ts_now.val < ts_next.val) {
			usleep(ts_next.val - ts_now.val);
			continue;
		}
		ts_next.val = ts_now.val + COGO_TICK_INTERVAL_US;

		pwm_set_duty(PWM_CH_COGO, anim_data[step]);
		step = (step + 1) % anim_points;
	}
}
