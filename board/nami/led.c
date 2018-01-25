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

#define LED_PULSE_US		(2 * SECOND)
/* 40 msec for nice and smooth transition. */
#define LED_PULSE_TICK_US	(40 * MSEC)

/* When pulsing is enabled, brightness is incremented by <duty_inc> every
 * <interval> usec from 0 to 100% in LED_PULSE_US usec. Then it's decremented
 * likewise in LED_PULSE_US usec. */
static struct {
	uint32_t interval;
	int duty_inc;
	enum led_color color;
	int duty;
} led_pulse;

#define CONFIGURE_PULSE(interval, color) \
	configure_pulse((interval), 100 / (LED_PULSE_US / (interval)), (color))

static void configure_pulse(uint32_t interval, int duty_inc,
	enum led_color color)
{
	led_pulse.interval = interval;
	led_pulse.duty_inc = duty_inc;
	led_pulse.color = color;
	led_pulse.duty = 0;
}

static void pulse_battery_led(enum led_color color)
{
	set_color(EC_LED_ID_BATTERY_LED, color, led_pulse.duty);
	if (led_pulse.duty + led_pulse.duty_inc > 100)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	else if (led_pulse.duty + led_pulse.duty_inc < 0)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	led_pulse.duty += led_pulse.duty_inc;
}

static void led_tick(void);
DECLARE_DEFERRED(led_tick);
static void led_tick(void)
{
	uint32_t elapsed;
	uint32_t next = 0;
	uint32_t start = get_time().le.lo;
	static uint8_t pwm_enabled = 0;

	if (!pwm_enabled) {
		pwm_enable(PWM_CH_LED_WHITE, 1);
		pwm_enable(PWM_CH_LED_AMBER, 1);
		pwm_enabled = 1;
	}
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		pulse_battery_led(led_pulse.color);
	elapsed = get_time().le.lo - start;
	next = led_pulse.interval > elapsed ? led_pulse.interval - elapsed : 0;
	hook_call_deferred(&led_tick_data, next);
}

static void board_led_set_battery(void)
{
	static uint8_t breath_enabled;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		hook_call_deferred(&led_tick_data, -1);
		set_color(EC_LED_ID_BATTERY_LED, LED_AMBER, 100);
		breath_enabled = 0;
		break;
	case PWR_STATE_DISCHARGE:
		hook_call_deferred(&led_tick_data, -1);
		set_color(EC_LED_ID_BATTERY_LED, LED_OFF, 0);
		breath_enabled = 0;
		break;
	case PWR_STATE_ERROR:
		if (!breath_enabled) {
			CONFIGURE_PULSE(LED_PULSE_TICK_US, LED_AMBER);
			led_tick();
			breath_enabled = 1;
		}
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		hook_call_deferred(&led_tick_data, -1);
		set_color(EC_LED_ID_BATTERY_LED, LED_WHITE, 100);
		breath_enabled = 0;
		break;
	default:
		/* Other states don't alter LED behavior */
		hook_call_deferred(&led_tick_data, -1);
		breath_enabled = 0;
		break;
	}
}

static uint32_t battery_interval;
static void battery_tick(void);
DECLARE_DEFERRED(battery_tick);
static void battery_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		board_led_set_battery();
	hook_call_deferred(&battery_tick_data, battery_interval);
}

void battery_alert(uint32_t interval)
{
	battery_interval = interval;
	battery_tick();
}

static int command_led(int argc, char **argv)
{
	enum ec_led_id id = EC_LED_ID_BATTERY_LED;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "debug")) {
		led_auto_control(id, !led_auto_control_is_enabled(id));
		ccprintf("o%s\n", led_auto_control_is_enabled(id) ? "ff" : "n");
	} else if (!strcasecmp(argv[1], "off")) {
		set_color(id, LED_OFF, 0);
	} else if (!strcasecmp(argv[1], "white")) {
		set_color(id, LED_WHITE, 100);
	} else if (!strcasecmp(argv[1], "amber")) {
		set_color(id, LED_AMBER, 100);
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"[debug|white|amber|off]",
			"Turn on/off LED.");

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
