/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Sasukette
 *
 */

#include "chipset.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "pwm.h"

#define LED_OFF_LVL	1
#define LED_ON_LVL	0

__override const int led_charge_lvl_1 = 1;

__override const int led_charge_lvl_2 = 100;

/* Sasukette : There are 3 leds for AC, Battery and Power */
__override struct led_descriptor
			led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
	[STATE_CHARGING_LVL_1] = {{EC_LED_COLOR_RED, 1 * LED_ONE_SEC},
			{LED_OFF, 1 * LED_ONE_SEC} },
	[STATE_CHARGING_LVL_2] = {{EC_LED_COLOR_RED, LED_INDEFINITE} },
	[STATE_CHARGING_FULL_CHARGE] = {{EC_LED_COLOR_GREEN, LED_INDEFINITE} },
	[STATE_DISCHARGE_S0] = {{EC_LED_COLOR_BLUE, LED_INDEFINITE} },
	[STATE_DISCHARGE_S0_BAT_LOW] = {{LED_OFF, LED_INDEFINITE} },
	[STATE_DISCHARGE_S3] = {{LED_OFF, LED_INDEFINITE} },
	[STATE_DISCHARGE_S5] = {{LED_OFF, LED_INDEFINITE} },
	[STATE_BATTERY_ERROR] = {{EC_LED_COLOR_RED, 0.5 * LED_ONE_SEC},
			{LED_OFF, 0.5 * LED_ONE_SEC} },
	[STATE_FACTORY_TEST] = {{EC_LED_COLOR_RED, 1 * LED_ONE_SEC},
			{LED_OFF, 1 * LED_ONE_SEC} },
};

__override const struct led_descriptor
		led_pwr_state_table[PWR_LED_NUM_STATES][LED_NUM_PHASES] = {
	[PWR_LED_STATE_ON]           = {{EC_LED_COLOR_BLUE, LED_INDEFINITE} },
	[PWR_LED_STATE_SUSPEND_AC]   = {{LED_OFF, LED_INDEFINITE} },
	[PWR_LED_STATE_SUSPEND_NO_AC] = {{LED_OFF, LED_INDEFINITE} },
	[PWR_LED_STATE_OFF]           = {{LED_OFF, LED_INDEFINITE} },
};

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

__override void led_set_color_power(enum ec_led_colors color)
{
	/* Don't set led if led_auto_control is disabled. */
	if (!led_auto_control_is_enabled(EC_LED_ID_POWER_LED) ||
		!led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		return;
	}

	if (color == EC_LED_COLOR_BLUE) {
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		pwm_set_duty(PWM_CH_LED_BLUE, 22);
	} else {
		/* LED_OFF and unsupported colors */
		pwm_set_duty(PWM_CH_LED_BLUE, 0);
	}
}

__override void led_set_color_battery(enum ec_led_colors color)
{
	/* Don't set led if led_auto_control is disabled. */
	if (!led_auto_control_is_enabled(EC_LED_ID_POWER_LED) ||
		!led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		return;
	}
	if (pwm_get_duty(PWM_CH_LED_BLUE) != 0) {
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
	}

	switch (color) {
	case EC_LED_COLOR_GREEN:
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 12);
		break;
	case EC_LED_COLOR_RED:
		pwm_set_duty(PWM_CH_LED_RED, 10);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		break;
	default: /* LED_OFF and other unsupported colors */
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		break;
	}
}


void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_GREEN] = 12;
		brightness_range[EC_LED_COLOR_RED] = 10;
	} else if (led_id == EC_LED_ID_POWER_LED) {
		brightness_range[EC_LED_COLOR_BLUE] = 22;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_RED]) {
		pwm_set_duty(PWM_CH_LED_RED, brightness[EC_LED_COLOR_RED]);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		pwm_set_duty(PWM_CH_LED_BLUE, 0);
	} else if (brightness[EC_LED_COLOR_GREEN]) {
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, brightness[EC_LED_COLOR_GREEN]);
		pwm_set_duty(PWM_CH_LED_BLUE, 0);
	} else if (brightness[EC_LED_COLOR_BLUE]) {
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		pwm_set_duty(PWM_CH_LED_BLUE, brightness[EC_LED_COLOR_BLUE]);
	} else {
		pwm_set_duty(PWM_CH_LED_RED, 0);
		pwm_set_duty(PWM_CH_LED_GREEN, 0);
		pwm_set_duty(PWM_CH_LED_BLUE, 0);
	}

	return EC_SUCCESS;
}
