/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED and Power LED control for NIKE
 */

#include "gpio.h"
#include "hooks.h"
#include "battery.h"
#include "charge_state.h"
#include "chipset.h"

enum led_color {
	LED_OFF = 0,
	LED_BLUE,
	LED_ORANGE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int bat_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_CHARGING, 0);
		gpio_set_level(GPIO_BAT_LED1, 0);
		break;
	case LED_BLUE:
		gpio_set_level(GPIO_CHARGING, 0);
		gpio_set_level(GPIO_BAT_LED1, 1);
		break;
	case LED_ORANGE:
		gpio_set_level(GPIO_CHARGING, 1);
		gpio_set_level(GPIO_BAT_LED1, 0);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int pwr_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_LED_POWER_L, 0);
		gpio_set_level(GPIO_PWR_LED0, 0);
		break;
	case LED_BLUE:
		gpio_set_level(GPIO_LED_POWER_L, 1);
		gpio_set_level(GPIO_PWR_LED0, 0);
		break;
	case LED_ORANGE:
		gpio_set_level(GPIO_LED_POWER_L, 0);
		gpio_set_level(GPIO_PWR_LED0, 1);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/**  * Called by hook task every 1 sec  */
static void led_second(void)
{
	static int second;

	second++;

	/* PWR LED behavior:
	 * Power on: Blue
	 * Suspand: Breeze mode Orange ( 1 sec on/ 3 sec off)
	 * Power off: OFF
	 */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		pwr_led_set_color(LED_OFF);
	else if (chipset_in_state(CHIPSET_STATE_ON))
		pwr_led_set_color(LED_BLUE);
	else if (chipset_in_state(CHIPSET_STATE_SUSPEND))
		pwr_led_set_color((second & 3) ? LED_OFF : LED_ORANGE);

	/* BAT LED behavior:
	 * Fully charged: Blue
	 * Under charging: Orange
	 * Battery low (10%): Breeze mode Orange (1 sec on, 3 sec off)
	 * Battery critical low (less than 3%) or
	 * abnormal battery situation:
	 * Blinking mode Orange: (1 sec on, 1 sec off)
	 * Using battery or not connected to AC power: OFF
	 */
	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		bat_led_set_color(LED_ORANGE);
		break;
	case PWR_STATE_DISCHARGE:
		if (charge_get_percent() < 3)
			bat_led_set_color((second & 1) ? LED_OFF : LED_ORANGE);
		else if (charge_get_percent() < 10)
			bat_led_set_color((second & 3) ? LED_OFF : LED_ORANGE);
		else
			bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		bat_led_set_color((second & 1) ? LED_OFF : LED_ORANGE);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		bat_led_set_color(LED_BLUE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

