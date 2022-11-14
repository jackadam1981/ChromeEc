/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include "compile_time_macros.h"
#include "hooks.h"
#include "pwm.h"
#include "pwm_chip.h"

const struct pwm_t pwm_channels[] = {
	[PWM_CH_FAN] = {
		.channel = 5,
		.flags = PWM_CONFIG_OPEN_DRAIN,
		.freq = 25000,
	},
	[PWM_CH_LED4] = {
		.channel = 7,
		.flags = PWM_CONFIG_ACTIVE_LOW | PWM_CONFIG_DSLEEP,
		.freq = 4800,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

static void board_pwm_init(void)
{
	pwm_enable(PWM_CH_KBLIGHT, 1);
	pwm_set_duty(PWM_CH_KBLIGHT, 50);
}
DECLARE_HOOK(HOOK_INIT, board_pwm_init, HOOK_PRIO_DEFAULT);
