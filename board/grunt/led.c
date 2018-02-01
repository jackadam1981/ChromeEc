/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Grunt
 */

#include "battery.h"
#include "charge_state.h"
#include "chipset.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "led_common.h"
#include "pwm.h"
#include "util.h"

#define BAT_LED_ON 0
#define BAT_LED_OFF 1

#define PWR_LED_ON 0
#define PWR_LED_OFF 1

#define CRITICAL_LOW_BATTERY_PERCENTAGE 3
#define LOW_BATTERY_PERCENTAGE 10

#define LED_TOTAL_4SECS_TICKS 4
#define LED_TOTAL_2SECS_TICKS 2
#define LED_ON_1SEC_TICKS 1
#define LED_ON_2SECS_TICKS 2

const enum ec_led_id supported_led_ids[] = {
			EC_LED_ID_POWER_LED, EC_LED_ID_BATTERY_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_BLUE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int led_set_color_battery(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_LED1_AMBER, 0);
		pwm_set_duty(PWM_CH_LED2_BLUE, 0);
		break;
	case LED_BLUE:
		pwm_set_duty(PWM_CH_LED1_AMBER, 0);
		pwm_set_duty(PWM_CH_LED2_BLUE, 100);
		break;
	case LED_AMBER:
		pwm_set_duty(PWM_CH_LED1_AMBER, 100);
		pwm_set_duty(PWM_CH_LED2_BLUE, 0);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int led_set_color_power(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_LED2_BLUE, 0);
		break;
	case LED_BLUE:
		pwm_set_duty(PWM_CH_LED2_BLUE, 100);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_BLUE] = 1;
	brightness_range[EC_LED_COLOR_AMBER] = 1;
}

static int led_set_color(enum ec_led_id led_id, enum led_color color)
{
	int rv;

	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		rv = led_set_color_battery(color);
		break;
	case EC_LED_ID_POWER_LED:
		rv = led_set_color_power(color);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return rv;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_BLUE] != 0)
		led_set_color(led_id, LED_BLUE);
	else if (brightness[EC_LED_COLOR_AMBER] != 0)
		led_set_color(led_id, LED_AMBER);
	else
		led_set_color(led_id, LED_OFF);

	return EC_SUCCESS;
}

static void led_set_battery(void)
{
	static int battery_ticks;

	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		led_set_color_battery(LED_AMBER);
		break;
	case PWR_STATE_DISCHARGE:
		led_set_color_battery(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		led_set_color_battery(
			(battery_ticks % LED_TOTAL_2SECS_TICKS <
			 LED_ON_1SEC_TICKS) ? LED_AMBER : LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		led_set_color_battery(LED_BLUE);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			led_set_color_battery(
				(battery_ticks % LED_TOTAL_4SECS_TICKS <
				 LED_ON_2SECS_TICKS) ? LED_AMBER : LED_BLUE);
		else
			led_set_color_battery(LED_BLUE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void led_set_power(void)
{
	static int suspend_ticks;
	static int previous_state_suspend;

	suspend_ticks++;

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
		if (!previous_state_suspend)
			suspend_ticks = 0;
		/* Blink once every four seconds. */
		led_set_color_battery(
			(suspend_ticks % LED_TOTAL_4SECS_TICKS)
			< LED_ON_1SEC_TICKS ?
			LED_BLUE : LED_OFF);
		previous_state_suspend = 1;
		return;
	}

	previous_state_suspend = 0;

	if (chipset_in_state(CHIPSET_STATE_ON))
		led_set_color_power(LED_BLUE);
	else
		led_set_color_power(LED_OFF);
}

/* Called by hook task every 1 sec */
static void led_second(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		led_set_battery();
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_power();
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

void led_control(enum ec_led_id led_id, enum ec_led_state state)
{
	/* TODO(sjg@chromium.org): Figure out what we should do here */
	led_second();
}
