/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED and Power LED control for speedy
 */

#include "gpio.h"
#include "hooks.h"
#include "battery.h"
#include "charge_state.h"
#include "chipset.h"
#include "led_common.h"
#include "util.h"
#include "include/extpower.h"
#include "lid_switch.h"
#include "console.h"

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

static void bat_led_set_green(void)
{
	gpio_set_level(GPIO_BAT_LED1, 0);
	gpio_set_level(GPIO_BAT_LED0, 1);
}

static void bat_led_set_orange(void)
{
	gpio_set_level(GPIO_BAT_LED0, 0);
	gpio_set_level(GPIO_BAT_LED1, 1);
}

static void bat_led_set_off(void)
{
	gpio_set_level(GPIO_BAT_LED0, 1);
	gpio_set_level(GPIO_BAT_LED1, 1);
}

static int pwr_led_set(int on)
{
	gpio_set_level(GPIO_POWER_LED, on ? 0 : 1);
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	/* Ignoring led_id as both leds support the same colors */
	brightness_range[EC_LED_COLOR_GREEN] = 1;
	brightness_range[EC_LED_COLOR_YELLOW] = 1;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		if (brightness[EC_LED_COLOR_GREEN] != 0) {
			bat_led_set_green();
		} else if (brightness[EC_LED_COLOR_YELLOW] != 0) {
			bat_led_set_orange();
		} else {
			bat_led_set_off();
		}
		break;
	case EC_LED_ID_POWER_LED:
		pwr_led_set(brightness[EC_LED_COLOR_BLUE]);
		break;
	default:
		return EC_ERROR_UNKNOWN;

	}
	return EC_SUCCESS;
}

static void speedy_led_set_power(void)
{
	static int power_second;

	/* Power LED status
	 *
	 * S0:
	 *   lid open: Green (solid)
	 *   lid closed (docked mode): off
	 *
	 * S3:
	 *   lid open: Green (Blinking) flashing every 2 seconds
	 *   lid close:  off
	 *
	 * S5: off
	 */
	if (lid_is_open()) {
		power_second++;

		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			pwr_led_set(0);
		else if (chipset_in_state(CHIPSET_STATE_ON))
			pwr_led_set(1);
		else if (chipset_in_state(CHIPSET_STATE_SUSPEND)) {
			pwr_led_set(power_second & 4 ? 0 : 1);
		}
	}
}

static void speedy_led_set_battery(void)
{
	static int battery_second;
	int battery_percent;

	battery_second++;

	/* Charging LED status
	 *
	 * When battery is lower than 10% and external power disconnected: Orange (Blinking)
	 *   flashing every 2 seconds
	 * When battery is 10%~95% and external power connected: Orange (solid)
	 * When battery is 95%~100% and external power connected: Green (solid)
	 * When battery error: flash Orange quickly flashing every 0.5 seconds
	 */

	if (charge_get_state() == PWR_STATE_ERROR) {
		if (battery_second & 1)
			bat_led_set_orange();
		else
			bat_led_set_off();
	}
	else {
		battery_percent = charge_get_percent();

		if (battery_percent < 10) {
			if (battery_second & 4)
				bat_led_set_orange();
			else
				bat_led_set_off();
		}
		else if (extpower_is_present()) {
			if (battery_percent > 95)
				bat_led_set_green();
			else
				bat_led_set_orange();
		}
		else {
			bat_led_set_off();
		}
	}
}

/**  * Called by hook task every 500 ms  */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		speedy_led_set_power();
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		speedy_led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

