/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Brya specific PWM LED settings: there are 2 LEDs on each side of the board,
 * each one can be controlled separately. The LED colors are white or amber,
 * and the default behavior is tied to the charging process: both sides are
 * amber while charging the battery and white when the battery is charged.
 */

#include <stdint.h>

#include "common.h"
#include "compile_time_macros.h"
#include "ec_commands.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "pwm.h"
#include "util.h"

#define BAT_LED_ON_LVL 50
#define BAT_LED_OFF_LVL 0
#define PWR_LED_ON_LVL 50
#define PWR_LED_OFF_LVL 0

__override const int led_charge_lvl_1 = 5;
__override const int led_charge_lvl_2 = 100;

__override struct led_descriptor
			led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
	[STATE_CHARGING_LVL_1]       = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[STATE_CHARGING_LVL_2]       = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[STATE_CHARGING_FULL_CHARGE] = {{LED_OFF,  LED_INDEFINITE} },
	[STATE_DISCHARGE_S0]         = {{LED_OFF,  LED_INDEFINITE} },
	[STATE_DISCHARGE_S0_BAT_LOW] = {{EC_LED_COLOR_AMBER,  LED_INDEFINITE} },
	[STATE_DISCHARGE_S3]         = {{LED_OFF,  LED_INDEFINITE} },
	[STATE_DISCHARGE_S5]         = {{LED_OFF,  LED_INDEFINITE} },
	[STATE_BATTERY_ERROR]        = {{EC_LED_COLOR_AMBER,  1 * LED_ONE_SEC},
					{LED_OFF,  1 * LED_ONE_SEC} },
	[STATE_FACTORY_TEST]         = {{EC_LED_COLOR_WHITE, 1 * LED_ONE_SEC},
					{LED_OFF,  1 * LED_ONE_SEC} },
};

__override const struct led_descriptor
		led_pwr_state_table[PWR_LED_NUM_STATES][LED_NUM_PHASES] = {
	[PWR_LED_STATE_ON]           = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[PWR_LED_STATE_SUSPEND_AC]   = {{EC_LED_COLOR_WHITE, 1 * LED_ONE_SEC},
					{LED_OFF,  1 * LED_ONE_SEC} },
	[PWR_LED_STATE_OFF]          = {{LED_OFF, LED_INDEFINITE} },
};

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

__override void led_set_color_battery(enum ec_led_colors color)
{
	int led1_duty, led2_duty;

	led1_duty = led2_duty = BAT_LED_OFF_LVL;

	switch (color) {
	case EC_LED_COLOR_AMBER:
		led1_duty = BAT_LED_ON_LVL;
		break;
	case EC_LED_COLOR_WHITE:
		led2_duty = BAT_LED_ON_LVL;
		break;
	default: /* LED_OFF and other unsupported colors */
		break;
	}

	pwm_set_duty(PWM_CH_LED1, led1_duty);
	pwm_set_duty(PWM_CH_LED2, led2_duty);
}

__override void led_set_color_power(enum ec_led_colors color)
{
	int led3_duty, led4_duty;

	led3_duty = led4_duty = PWR_LED_OFF_LVL;

	switch (color) {
	case EC_LED_COLOR_AMBER:
		led3_duty = PWR_LED_ON_LVL;
		break;
	case EC_LED_COLOR_WHITE:
		led4_duty = PWR_LED_ON_LVL;
		break;
	default: /* LED_OFF and other unsupported colors */
		break;
	}

	pwm_set_duty(PWM_CH_LED3, led3_duty);
	pwm_set_duty(PWM_CH_LED4, led4_duty);
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_AMBER] = 1;
	brightness_range[EC_LED_COLOR_WHITE] = 1;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(EC_LED_COLOR_AMBER);
		else if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(EC_LED_COLOR_WHITE);
		else
			led_set_color_battery(LED_OFF);
	}	else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_power(EC_LED_COLOR_AMBER);
		else if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_power(EC_LED_COLOR_WHITE);
		else
			led_set_color_power(LED_OFF);
	}
	return EC_SUCCESS;
}
