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
#include "util.h"

/* rev0 has no LEDs */
#ifndef POPPY_REV0

#define BAT_LED_ON 1
#define BAT_LED_OFF 0

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_LEFT_LED,
	EC_LED_ID_RIGHT_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_WHITE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int side_led_set_color(int port, enum led_color color)
{
	enum gpio_signal yellow = port ? GPIO_LED_YELLOW_C1 :
		GPIO_LED_YELLOW_C0;
	enum gpio_signal white = port ? GPIO_LED_WHITE_C1 :
		GPIO_LED_WHITE_C0;

	switch (color) {
	case LED_OFF:
		gpio_set_level(yellow, BAT_LED_OFF);
		gpio_set_level(white, BAT_LED_OFF);
		break;
	case LED_AMBER:
		gpio_set_level(yellow, BAT_LED_ON);
		gpio_set_level(white, BAT_LED_OFF);
		break;
	case LED_WHITE:
		gpio_set_level(yellow, BAT_LED_OFF);
		gpio_set_level(white, BAT_LED_ON);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_YELLOW] = 1;
	brightness_range[EC_LED_COLOR_WHITE] = 1;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	int port;

	switch (led_id) {
	case EC_LED_ID_LEFT_LED:
		port = 0;
		break;
	case EC_LED_ID_RIGHT_LED:
		port = 1;
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	if (brightness[EC_LED_COLOR_WHITE] != 0)
		side_led_set_color(port, LED_WHITE);
	else if (brightness[EC_LED_COLOR_YELLOW] != 0)
		side_led_set_color(port, LED_AMBER);
	else
		side_led_set_color(port, LED_OFF);

	return EC_SUCCESS;
}

static void board_led_set_battery(void)
{
	static int battery_ticks;
	uint32_t chflags = charge_get_flags();
	int port;

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		port = charge_manager_get_active_charge_port();
		/* Always indicate when charging, even in suspend. */
		side_led_set_color(port, LED_AMBER);
		side_led_set_color(!port, LED_OFF);
		break;
	case PWR_STATE_DISCHARGE:
		/*
		 * TODO(b/37970194): Do we really want to blink on low battery?
		 * If yes, what's the threshold? In S0 only?
		 */
		if (charge_get_percent() < 12)
			side_led_set_color(0,
				(battery_ticks & 0x4) ? LED_WHITE : LED_OFF);
		else
			side_led_set_color(0, LED_OFF);

		side_led_set_color(1, LED_OFF);
		break;
	case PWR_STATE_ERROR:
		side_led_set_color(0,
				(battery_ticks & 0x2) ? LED_WHITE : LED_OFF);
		side_led_set_color(1, LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		port = charge_manager_get_active_charge_port();
		side_led_set_color(port, LED_WHITE);
		side_led_set_color(!port, LED_OFF);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		port = charge_manager_get_active_charge_port();
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			side_led_set_color(port,
				(battery_ticks & 0x4) ? LED_AMBER : LED_OFF);
		else
			side_led_set_color(port, LED_WHITE);

		side_led_set_color(!port, LED_OFF);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		board_led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

#endif /* !POPPY_REV0 */
