/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power and battery LED control for Nami
 */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

#define LED_TOTAL_TICKS 8
#define LED_ON_TICKS 4

const enum ec_led_id supported_led_ids[] = {EC_LED_ID_BATTERY_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_AMBER,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

static int set_color_battery(enum led_color color, int duty)
{
	if (duty < 0 || 100 < duty)
		return EC_ERROR_UNKNOWN;

	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_LED_WHITE, 0);
		pwm_set_duty(PWM_CH_LED_AMBER, 0);
		break;
	case LED_WHITE:
		pwm_set_duty(PWM_CH_LED_WHITE, duty);
		pwm_set_duty(PWM_CH_LED_AMBER, 0);
		break;
	case LED_AMBER:
		pwm_set_duty(PWM_CH_LED_WHITE, 0);
		pwm_set_duty(PWM_CH_LED_AMBER, duty);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int set_color(enum ec_led_id id, enum led_color color, int duty)
{
	switch (id) {
	case EC_LED_ID_BATTERY_LED:
		return set_color_battery(color, duty);
	default:
		return EC_ERROR_UNKNOWN;
	}
}

static void board_led_set_battery(void)
{
	static unsigned int battery_ticks;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		set_color(EC_LED_ID_BATTERY_LED, LED_AMBER, 100);
		break;
	case PWR_STATE_DISCHARGE:
		set_color(EC_LED_ID_BATTERY_LED, LED_OFF, 0);
		break;
	case PWR_STATE_ERROR:
		if ((battery_ticks++ % LED_TOTAL_TICKS) < LED_ON_TICKS)
			set_color(EC_LED_ID_BATTERY_LED, LED_AMBER, 100);
		else
			set_color(EC_LED_ID_BATTERY_LED, LED_OFF, 0);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		set_color(EC_LED_ID_BATTERY_LED, LED_WHITE, 100);
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

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_WHITE] = 100;
	brightness_range[EC_LED_COLOR_AMBER] = 100;
}

int led_set_brightness(enum ec_led_id id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_WHITE])
		return set_color(id, LED_WHITE, brightness[EC_LED_COLOR_WHITE]);
	else if (brightness[EC_LED_COLOR_AMBER])
		return set_color(id, LED_AMBER, brightness[EC_LED_COLOR_AMBER]);
	else
		return set_color(id, LED_OFF, 0);
}
