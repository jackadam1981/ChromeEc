/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Volteer
 */

#include "charge_manager.h"
#include "common.h"
#include "ec_commands.h"
#include "hooks.h"
#include "led_common.h"
#include "gpio.h"
#include "charge_state.h"
#include "chipset.h"

#define BAT_LED_ON 0
#define BAT_LED_OFF 1

#define POWER_LED_ON 1
#define POWER_LED_OFF 0

#define LED_TICKS_PER_CYCLE 10
#define LED_ON_TICKS 5

#define LED_BATTERY_LOW_LEVEL	10	/* Battery low level: 10% */

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_LEFT_LED,
	EC_LED_ID_RIGHT_LED,
	EC_LED_ID_POWER_LED
};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_WHITE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

enum led_port {
	RIGHT_PORT = 0,
	LEFT_PORT
};

void led_set_color_power(enum ec_led_colors color)
{
	switch (color) {
	case LED_OFF:
		gpio_set_level(GPIO_PWR_LED_WHITE_H, POWER_LED_OFF);
		break;
	case LED_WHITE:
		gpio_set_level(GPIO_PWR_LED_WHITE_H, POWER_LED_ON);
		break;
	default:
		break;
	}
}

static void led_set_color_battery(int port, enum led_color color)
{
	int side_select;

	side_select = (port == RIGHT_PORT ? 1 : 0);
	gpio_set_level(GPIO_LED_SIDESEL, side_select);

	switch (color) {
	case LED_WHITE:
		gpio_set_level(GPIO_LED_WHITE_L, BAT_LED_ON);
		gpio_set_level(GPIO_LED_AMBER_L, BAT_LED_OFF);
		break;
	case LED_AMBER:
		gpio_set_level(GPIO_LED_WHITE_L, BAT_LED_OFF);
		gpio_set_level(GPIO_LED_AMBER_L, BAT_LED_ON);
		break;
	case LED_OFF:
		gpio_set_level(GPIO_LED_WHITE_L, BAT_LED_OFF);
		gpio_set_level(GPIO_LED_AMBER_L, BAT_LED_OFF);
		break;
	default:
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_LEFT_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		break;
	case EC_LED_ID_RIGHT_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		break;
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		break;
	default:
		break;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_LEFT_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(1, LED_WHITE);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(1, LED_AMBER);
		else
			led_set_color_battery(1, LED_OFF);
		break;
	case EC_LED_ID_RIGHT_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(0, LED_WHITE);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(0, LED_AMBER);
		else
			led_set_color_battery(0, LED_OFF);
		break;
	case EC_LED_ID_POWER_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_power(LED_WHITE);
		else
			led_set_color_power(LED_OFF);
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}

/* Illuminates the LED on the side of the active charging port.
 */
static void set_active_port_color(enum led_color color)
{
	int port = charge_manager_get_active_charge_port();

	led_set_color_battery(port, color);
}

static void led_set_battery(void)
{
	static int battery_ticks;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		set_active_port_color(LED_AMBER);
		break;
	case PWR_STATE_DISCHARGE:
		if (led_auto_control_is_enabled(EC_LED_ID_RIGHT_LED)) {
			if (charge_get_percent() < LED_BATTERY_LOW_LEVEL)
				led_set_color_battery(0, (battery_ticks %
					LED_TICKS_PER_CYCLE < LED_ON_TICKS) ?
					LED_WHITE : LED_OFF);
			else
				led_set_color_battery(0, LED_OFF);
		}
		break;
	case PWR_STATE_ERROR:
		set_active_port_color((battery_ticks & 0x2) ?
				LED_WHITE : LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		set_active_port_color(LED_WHITE);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			set_active_port_color((battery_ticks %
				LED_TICKS_PER_CYCLE < LED_ON_TICKS) ?
				LED_AMBER : LED_OFF);
		else
			set_active_port_color(LED_WHITE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void led_set_power(void)
{
	static int power_tick;

	power_tick++;

	if (chipset_in_state(CHIPSET_STATE_ON))
		led_set_color_power(LED_WHITE);
	else if (chipset_in_state(CHIPSET_STATE_SUSPEND |
				  CHIPSET_STATE_STANDBY))
		led_set_color_power((power_tick %
			LED_TICKS_PER_CYCLE < LED_ON_TICKS) ?
			LED_WHITE : LED_OFF);
	else
		led_set_color_power(LED_OFF);
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_power();

	led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);
