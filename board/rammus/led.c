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
#define LED_CHARGE_PULSE 8
#define LED_POWER_PULSE 12

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_POWER_LED,
	EC_LED_ID_BATTERY_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_GREEN,
	LED_AMBER,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

enum led_charge_state {
	LED_STATE_DISCHARGE = 0,
	LED_STATE_CHARGE,
	LED_STATE_FULL,
	LED_STATE_ERROR,
	LED_CHARGE_STATE_COUNT
};

enum led_power_state {
	LED_STATE_S0 = 0,
	LED_STATE_S3,
	LED_STATE_S5,
	LED_POWER_STATE_COUNT
};

static void set_color_battery(enum led_color color)
{
	if (color == LED_GREEN) {
		gpio_set_level(GPIO_CHG_LED1, 1);
		gpio_set_level(GPIO_CHG_LED2, 0);
	}

	if (color == LED_AMBER) {
		gpio_set_level(GPIO_CHG_LED1, 0);
		gpio_set_level(GPIO_CHG_LED2, 1);
	}

	if (color == LED_OFF) {
		gpio_set_level(GPIO_CHG_LED1, 0);
		gpio_set_level(GPIO_CHG_LED2, 0);
	}
}

static void set_color_power(enum led_color color)
{
	if (color == LED_WHITE)
		gpio_set_level(GPIO_PWR_LED, 1);

	if (color == LED_OFF)
		gpio_set_level(GPIO_PWR_LED, 0);
}

static void set_color(enum ec_led_id id, enum led_color color)
{
	if (id == EC_LED_ID_BATTERY_LED)
		set_color_battery(color);
	else
		set_color_power(color);
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

void config_power_led(enum led_power_state state)
{
	static unsigned int power_ticks;

	switch (state) {
	case LED_STATE_S0:
		set_color(EC_LED_ID_POWER_LED, LED_WHITE);
		break;
	case LED_STATE_S3:
		if ((power_ticks++ % LED_TOTAL_TICKS) < LED_POWER_PULSE)
			set_color(EC_LED_ID_POWER_LED, LED_OFF);
		else
			set_color(EC_LED_ID_POWER_LED, LED_WHITE);
		break;
	case LED_STATE_S5:
		set_color(EC_LED_ID_POWER_LED, LED_OFF);
		break;
	default:
		set_color(EC_LED_ID_POWER_LED, LED_OFF);
		break;
	}

	if (state != LED_STATE_S3)
		power_ticks = 0;
}

void config_battery_led(enum led_charge_state state)
{
	static unsigned int charge_ticks;

	switch (state) {
	case LED_STATE_DISCHARGE:
		set_color(EC_LED_ID_BATTERY_LED, LED_OFF);
		break;
	case LED_STATE_CHARGE:
		set_color(EC_LED_ID_BATTERY_LED, LED_AMBER);
		break;
	case LED_STATE_FULL:
		set_color(EC_LED_ID_BATTERY_LED, LED_GREEN);
		break;
	case LED_STATE_ERROR:
		if ((charge_ticks++ % LED_TOTAL_TICKS) < LED_CHARGE_PULSE)
			set_color(EC_LED_ID_BATTERY_LED, LED_OFF);
		else
			set_color(EC_LED_ID_BATTERY_LED, LED_AMBER);
		break;
	default:
		set_color(EC_LED_ID_BATTERY_LED, LED_OFF);
	}

	if (state != LED_STATE_ERROR)
		charge_ticks = 0;
}

static void rammus_led_set_power(void)
{
	if (chipset_in_state(CHIPSET_STATE_ON))
		config_power_led(LED_STATE_S0);
	else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
		config_power_led(LED_STATE_S3);
	else
		config_power_led(LED_STATE_S5);
}

static void rammus_led_set_battery(void)
{
	enum charge_state chg_state = charge_get_state();
	int charge_percent = charge_get_percent();

	/* CHIPSET_STATE_OFF */
	switch (chg_state) {
	case PWR_STATE_DISCHARGE:
		if ((charge_get_flags() & CHARGE_FLAG_EXTERNAL_POWER) &&
			charge_percent >= BATTERY_LEVEL_NEAR_FULL)
			config_battery_led(LED_STATE_FULL);
		else
			config_battery_led(LED_STATE_DISCHARGE);
		break;
	case PWR_STATE_CHARGE:
		config_battery_led(LED_STATE_CHARGE);
		break;
	case PWR_STATE_ERROR:
		config_battery_led(LED_STATE_ERROR);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE:
		if(charge_get_flags() & CHARGE_FLAG_EXTERNAL_POWER)
			config_battery_led(LED_STATE_FULL);
		else
			config_battery_led(LED_STATE_OFF);
		break;
	default:
		break;
	}
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
