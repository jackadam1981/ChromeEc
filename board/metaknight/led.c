/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Metaknight
 */

#include "ec_commands.h"
#include "gpio.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "chipset.h"
#include "charge_state.h"
#include "hooks.h"
#include "pwm.h"
#include "task.h"


#define CPRINTS(format, args...) cprints(CC_TASK, format, ## args)

#define LED_ON_LVL 0
#define LED_OFF_LVL 1

__override const int led_charge_lvl_1;

__override const int led_charge_lvl_2 = 100;

__override struct led_descriptor
			led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
	[STATE_CHARGING_LVL_1]	     = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
	[STATE_CHARGING_LVL_2]	     = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
	[STATE_CHARGING_FULL_CHARGE] = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[STATE_DISCHARGE_S0]	     = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
	[STATE_DISCHARGE_S3]	     = {{EC_LED_COLOR_AMBER, 1 * LED_ONE_SEC},
					{LED_OFF,            3 * LED_ONE_SEC} },
	[STATE_DISCHARGE_S5]         = {{LED_OFF,            LED_INDEFINITE} },
	[STATE_BATTERY_ERROR]        = {{EC_LED_COLOR_AMBER, 1 * LED_ONE_SEC},
					{LED_OFF,            1 * LED_ONE_SEC} },
	[STATE_FACTORY_TEST]         = {{EC_LED_COLOR_WHITE, 2 * LED_ONE_SEC},
					{EC_LED_COLOR_AMBER, 2 * LED_ONE_SEC} },
};


const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

__override void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_WHITE:
		gpio_set_level(GPIO_LED_W_ODL, LED_ON_LVL);
		gpio_set_level(GPIO_LED_Y_ODL, LED_OFF_LVL);
		break;
	case EC_LED_COLOR_AMBER:
		gpio_set_level(GPIO_LED_W_ODL, LED_OFF_LVL);
		gpio_set_level(GPIO_LED_Y_ODL, LED_ON_LVL);
		break;
	default: /* LED_OFF and other unsupported colors */
		gpio_set_level(GPIO_LED_W_ODL, LED_OFF_LVL);
		gpio_set_level(GPIO_LED_Y_ODL, LED_OFF_LVL);
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(EC_LED_COLOR_WHITE);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(EC_LED_COLOR_AMBER);
		else
			led_set_color_battery(LED_OFF);
	}
	return EC_SUCCESS;
}

static void board_led_pwm_mode_disable(void)
{
	CPRINTS("%s", __func__);

	/* set LED pin as GPIO function */
	gpio_set_alternate_function(GPIO_PORT_C, (BIT(3) | BIT(4)), GPIO_ALT_FUNC_NONE);

	/* disable pwm */
	gpio_config_pin(MODULE_PWM, GPIO_LED_W_ODL, 0);
	gpio_config_pin(MODULE_PWM, GPIO_LED_Y_ODL, 0);

	/* enable gpio */
	gpio_config_pin(MODULE_GPIO, GPIO_LED_W_ODL, 1);
	gpio_config_pin(MODULE_GPIO, GPIO_LED_Y_ODL, 1);
}

static void board_led_pwm_mode_enable(void)
{
	CPRINTS("%s", __func__);

	/* set LED pin as alternate function */
	gpio_set_alternate_function(GPIO_PORT_C, (BIT(3) | BIT(4)), GPIO_ALT_FUNC_1);

	/* disable gpio */
	gpio_config_pin(MODULE_GPIO, GPIO_LED_W_ODL, 0);
	gpio_config_pin(MODULE_GPIO, GPIO_LED_Y_ODL, 0);

	/* enable pwm */
	gpio_config_pin(MODULE_PWM, GPIO_LED_W_ODL, 1);
	gpio_config_pin(MODULE_PWM, GPIO_LED_Y_ODL, 1);

	pwm_enable(PWM_CH_LED_AMBER, 1);
	pwm_enable(PWM_CH_LED_WHITE, 1);

	pwm_set_duty(PWM_CH_LED_AMBER, 0);
	pwm_set_duty(PWM_CH_LED_WHITE, 0);
}

static void board_led_mode_switch(void)
{
	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND) &&
		charge_get_state() == PWR_STATE_DISCHARGE) {

		board_led_pwm_mode_enable();
		task_wake(TASK_ID_LED_BREATHING);
	} else {
		board_led_pwm_mode_disable();
	}
}
DECLARE_DEFERRED(board_led_mode_switch);

static void board_led_mode_switch_check(void)
{
	hook_call_deferred(&board_led_mode_switch_data, (50 * MSEC));
}
DECLARE_HOOK(HOOK_AC_CHANGE, board_led_mode_switch_check, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_led_mode_switch_check, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_led_mode_switch_check, HOOK_PRIO_DEFAULT);

static void led_breathing(void)
{
	static int count = 100;
	static int state_up = 1;

	if (count < 100 && state_up)
		count++;
	else if (count == 100 && state_up) {
		count--;
		state_up = 0;
	}
	else if (count > 0 && !state_up) {
		count--;
	}
	else if (count == 0 && !state_up) {
		count++;
		state_up = 1;
	}

	pwm_set_duty(PWM_CH_LED_AMBER, count);
}

void led_breathing_task(void *u)
{
	while (1) {
		led_breathing();
		task_wait_event(20 * MSEC);
		if (!chipset_in_state(CHIPSET_STATE_ANY_SUSPEND) ||
			charge_get_state() != PWR_STATE_DISCHARGE)
			task_wait_event(-1);
	}
}