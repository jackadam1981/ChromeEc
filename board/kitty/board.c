/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kitty board-specific configuration */

#include "chipset.h"
#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "power.h"
#include "power_button.h"
#include "power.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "util.h"
#include "timer.h"

#define GPIO_INPUT_UP  (GPIO_INPUT | GPIO_PULL_UP)

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_SOC1V8_XPSHOLD, 1, "XPSHOLD"},
	{GPIO_SUSPEND_L,      0, "SUSPEND#_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(2), STM32_TIM_CH(3),
	 PWM_CONFIG_ACTIVE_LOW, GPIO_LED_POWER_L},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/*
 * kitty has no battery.
 * So implement an empty battery_wait_for_stable() function here.
 */
int battery_wait_for_stable(void)
{
	return EC_SUCCESS;
}
