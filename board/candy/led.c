/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Candy
 */

#include "charge_state.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "util.h"

const enum ec_led_id supported_led_ids[] = {EC_LED_ID_BATTERY_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_AMBER,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

static int bat_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_BAT_LED0, 1);
		gpio_set_level(GPIO_BAT_LED1, 1);
		break;
	case LED_WHITE:
		gpio_set_level(GPIO_BAT_LED0, 0);
		gpio_set_level(GPIO_BAT_LED1, 1);
		break;
	case LED_AMBER:
		gpio_set_level(GPIO_BAT_LED0, 1);
		gpio_set_level(GPIO_BAT_LED1, 0);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_WHITE] = 100;
	brightness_range[EC_LED_COLOR_YELLOW] = 100;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_WHITE] != 0)
		bat_led_set_color(LED_WHITE);
	else if (brightness[EC_LED_COLOR_YELLOW] != 0)
		bat_led_set_color(LED_AMBER);
	else
		bat_led_set_color(LED_OFF);

	return EC_SUCCESS;
}

static void candy_led_set_battery(void)
{
	static int battery_ticks;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		bat_led_set_color(LED_WHITE);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_DISCHARGE:
		if (chipset_in_state(CHIPSET_STATE_ON) ||
		    chipset_in_state(CHIPSET_STATE_SUSPEND))
			bat_led_set_color((charge_get_percent() < 15) ?
			LED_AMBER : LED_OFF);
		else
			bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		bat_led_set_color(
			(battery_ticks & 0x2) ? LED_AMBER : LED_OFF);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE. */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			bat_led_set_color(
			(battery_ticks & 0x4) ? LED_WHITE : LED_OFF);
		else
			bat_led_set_color(LED_OFF);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

/**
 * Called by hook task every 250 ms
 */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		candy_led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
