/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control.
 */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "led_common.h"
#include "system.h"
#include "util.h"

#define LED_TOTAL_TICKS 16
#define LED_ON_TICKS 8

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_POWER_LED,
	EC_LED_ID_BATTERY_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_PWR_OFF = 0,
	LED_CHG_OFF,
	LED_WHITE,
	LED_GREEN,
	LED_AMBER,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

/**
 * Set LED color
 *
 * @param color         Enumerated color value
 */
static void set_color(enum led_color color)
{
	if (color == LED_WHITE)
		gpio_set_level(GPIO_PWR_LED, 1);

	if (color == LED_GREEN) {
		gpio_set_level(GPIO_CHG_LED1, 1);
		gpio_set_level(GPIO_CHG_LED2, 0);
	}

	if (color == LED_AMBER) {
		gpio_set_level(GPIO_CHG_LED2, 1);
		gpio_set_level(GPIO_CHG_LED1, 0);
	}

	if (color == LED_PWR_OFF)
		gpio_set_level(GPIO_PWR_LED, 0);

	if (color == LED_CHG_OFF) {
		gpio_set_level(GPIO_CHG_LED1, 0);
		gpio_set_level(GPIO_CHG_LED2, 0);
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_WHITE] = 1;
	brightness_range[EC_LED_COLOR_GREEN] = 1;
	brightness_range[EC_LED_COLOR_AMBER] = 1;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	gpio_set_level(GPIO_PWR_LED, !brightness[EC_LED_COLOR_WHITE]);
	gpio_set_level(GPIO_CHG_LED1, !brightness[EC_LED_COLOR_GREEN]);
	gpio_set_level(GPIO_CHG_LED2, !brightness[EC_LED_COLOR_AMBER]);

	return EC_SUCCESS;
}


static void rammus_led_set_power(void)
{
	if (chipset_in_state(CHIPSET_STATE_ON))
		set_color(LED_WHITE);
	else
		set_color(LED_PWR_OFF);
}

static void rammus_led_set_battery(void)
{
	static unsigned int charge_ticks;
	enum led_color cur_led_color = LED_CHG_OFF;
	enum charge_state chg_state = charge_get_state();
	int charge_percent = charge_get_percent();

	/* CHIPSET_STATE_OFF */
	switch (chg_state) {
	case PWR_STATE_DISCHARGE:
		if ((charge_get_flags() & CHARGE_FLAG_EXTERNAL_POWER) &&
			charge_percent >= BATTERY_LEVEL_NEAR_FULL)
			cur_led_color = LED_GREEN;
		else
			cur_led_color = LED_CHG_OFF;
		break;
	case PWR_STATE_CHARGE:
		cur_led_color = LED_AMBER;
		break;
	case PWR_STATE_ERROR:
		cur_led_color = ((charge_ticks++ % LED_TOTAL_TICKS)
				< LED_ON_TICKS) ? LED_AMBER : LED_CHG_OFF;
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE:
		if(charge_get_flags() & CHARGE_FLAG_EXTERNAL_POWER)
			cur_led_color = LED_GREEN;
		else
			cur_led_color = LED_CHG_OFF;
		break;
	default:
		cur_led_color = LED_CHG_OFF;
		break;
	}

	set_color(cur_led_color);

	if (chg_state != PWR_STATE_ERROR)
		charge_ticks = 0;
}

/**
 * Called by hook task every 250 ms
 */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		rammus_led_set_power();

	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		rammus_led_set_battery();
}

DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
