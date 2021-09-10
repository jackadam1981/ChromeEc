/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>

#include "led_pwm.h"
#include "pwm.h"

#define DT_DRV_COMPAT cros_ec_pwm_leds

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(cros_ec_pwm_leds) <= 1,
	     "Multiple CrOS EC PWM LED instances defined");

#define PWM_CHANNEL_BY_IDX(led_num, led_ch)                        \
	PWM_CHANNEL(DT_PWMS_CTLR_BY_IDX(                           \
		DT_INST_PHANDLE_BY_IDX(0, leds, led_num), led_ch))

struct pwm_led pwm_leds[] = {
	[PWM_LED0] = {
		.ch0 = PWM_CHANNEL_BY_IDX(0, 0),
		.ch1 = PWM_CHANNEL_BY_IDX(0, 1),
		.ch2 = PWM_CHANNEL_BY_IDX(0, 2),
		.enable = &pwm_enable,
		.set_duty = &pwm_set_duty,
	},
#if DT_INST_PROP_LEN(0, leds) == 2
	[PWM_LED1] = {
		.ch0 = PWM_CHANNEL_BY_IDX(1, 0),
		.ch1 = PWM_CHANNEL_BY_IDX(1, 1),
		.ch2 = PWM_CHANNEL_BY_IDX(1, 2),
		.enable = &pwm_enable,
		.set_duty = &pwm_set_duty,
	},
#endif
};

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
