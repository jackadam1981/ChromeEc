/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM RGB LED control to conform to Chrome OS LED behaviour specification. */

/*
 * This assumes that a single RGB LED is shared between both power and
 * charging/battery status.  If multiple RGB LEDs are present, they all follow
 * the same patterns.
 */

#include "battery.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "led_common.h"
#include "led_rgb.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

/* Battery percentage thresholds to blink at different rates. */
#define CRITICAL_LOW_BATTERY_PERCENTAGE 3
#define LOW_BATTERY_PERCENTAGE 10

#define PULSE_TICK (250 * MSEC)

void set_rgb_led_color(enum rgb_led_id id, int color)
{
	if ((id > CONFIG_LED_RGB_COUNT) || (id < 0))
		return;

	if (color == -1) {
		pwm_set_duty(rgb_leds[id].red, 0);
		pwm_set_duty(rgb_leds[id].green, 0);
		pwm_set_duty(rgb_leds[id].blue, 0);
		return;
	}

	if (rgb_leds[id].red != RGB_LED_NO_CHANNEL)
		pwm_set_duty(rgb_leds[id].red, led_color_map[color].red);
	if (rgb_leds[id].green != RGB_LED_NO_CHANNEL)
		pwm_set_duty(rgb_leds[id].green, led_color_map[color].green);
	if (rgb_leds[id].blue != RGB_LED_NO_CHANNEL)
		pwm_set_duty(rgb_leds[id].blue, led_color_map[color].blue);
}

static void set_led_color(int color)
{
	if ((led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) ||
	    (led_auto_control_is_enabled(EC_LED_ID_LEFT_LED)))
		set_rgb_led_color(RGB_LED0, color);

#if CONFIG_LED_RGB_COUNT >= 2
	if (led_auto_control_is_enabled(EC_LED_ID_RIGHT_LED))
		set_rgb_led_color(RGB_LED1, color);
#endif /* CONFIG_RGB_LED_COUNT >= 2 */
}

static uint8_t show_suspend_state;
static uint8_t led_is_pulsing;
static uint8_t pulse_period;
static uint8_t pulse_ontime;
static enum ec_led_colors pulse_color;
static void pulse_leds_deferred(void);
DECLARE_DEFERRED(pulse_leds_deferred);
static void pulse_leds_deferred(void)
{
	static uint8_t tick_count;

	if (!led_is_pulsing && !show_suspend_state) {
		tick_count = 0;
		return;
	}

	if (tick_count < pulse_ontime)
		set_led_color(pulse_color);
	else
		set_led_color(-1);

	tick_count = (tick_count + 1) % pulse_period;
	hook_call_deferred(&pulse_leds_deferred_data, PULSE_TICK);
}

static void pulse_leds(enum ec_led_colors color, int ontime, int period)
{
	pulse_color = color;
	pulse_ontime = ontime;
	pulse_period = period;
	pulse_leds_deferred();
}

static void update_leds(void)
{
	enum charge_state chg_st = charge_get_state();
	int batt_percentage = charge_get_percent();

	/*
	 * Reflecting the charge state is the highest priority.
	 *
	 * Solid Amber == Charging
	 * Solid Green == Charging (near full)
	 * Fast Flash Red == Charging error or battery not present
	 * Slow Flash Amber == Low Battery
	 * Fast Flash Amber == Critical Battery
	 */
	if (chg_st == PWR_STATE_CHARGE) {
		led_is_pulsing = 0;
		show_suspend_state = 0;
		set_led_color(EC_LED_COLOR_AMBER);
	} else if (chg_st == PWR_STATE_CHARGE_NEAR_FULL) {
		led_is_pulsing = 0;
		show_suspend_state = 0;
		set_led_color(EC_LED_COLOR_GREEN);
	} else if ((battery_is_present() != BP_YES) ||
		   (chg_st == PWR_STATE_ERROR)) {
		/* 500 ms period, 50% duty cycle. */
		show_suspend_state = 0;
		led_is_pulsing = 1;
		pulse_leds(EC_LED_COLOR_RED, 1, 2);
	} else if (batt_percentage < CRITICAL_LOW_BATTERY_PERCENTAGE) {
		/* Flash amber faster (1 second period, 50% duty cycle) */
		show_suspend_state = 0;
		led_is_pulsing = 1;
		pulse_leds(EC_LED_COLOR_AMBER, 2, 4);
	} else if (batt_percentage < LOW_BATTERY_PERCENTAGE) {
		/* Flash amber (4 second period, 50% duty cycle) */
		show_suspend_state = 0;
		led_is_pulsing = 1;
		pulse_leds(EC_LED_COLOR_AMBER, 8, 16);
	} else {
		/* Discharging or not charging. Reflect the SoC state. */
		led_is_pulsing = 0;
		if (chipset_in_state(CHIPSET_STATE_ON)) {
			/* The LED must be on in the Active state. */
			show_suspend_state = 0;
			set_led_color(EC_LED_COLOR_GREEN);
		} else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
			/* The power LED must pulse in the suspend state. */
			if (!show_suspend_state) {
				/* 4 second period, 25% duty cycle. */
				show_suspend_state = 1;
				pulse_leds(EC_LED_COLOR_GREEN, 4, 16);
			}
		} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			/* Make sure we stop pulsing. */
			show_suspend_state = 0;
			/* The LED must be off in the Deep Sleep state. */
			set_led_color(-1);
		}
	}
}
DECLARE_HOOK(HOOK_TICK, update_leds, HOOK_PRIO_DEFAULT);

static void init_leds_off(void)
{
	set_led_color(-1);
}
DECLARE_HOOK(HOOK_INIT, init_leds_off, HOOK_PRIO_INIT_PWM + 1);

int command_ledtest(int argc, char **argv)
{
	int enable;
	int rgb_led_id;
	int led_id;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	if (!parse_bool(argv[2], &enable))
		return EC_ERROR_PARAM2;

	rgb_led_id = atoi(argv[1]);
	led_id = supported_led_ids[rgb_led_id];

	/* Inverted because this drives auto control. */
	led_auto_control(led_id, !enable);

	if (argc == 4) {
		/* Set the color or pattern. */
		if (!strncmp(argv[3], "red", 3))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_RED);
		else if (!strncmp(argv[3], "green", 5))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_GREEN);
		else if (!strncmp(argv[3], "amber", 5))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_AMBER);
		else if (!strncmp(argv[3], "blue", 4))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_BLUE);
		else if (!strncmp(argv[3], "white", 5))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_WHITE);
		else if (!strncmp(argv[3], "yellow", 6))
			set_rgb_led_color(rgb_led_id, EC_LED_COLOR_YELLOW);
		else if (!strncmp(argv[3], "off", 3))
			set_rgb_led_color(rgb_led_id, -1);
		else
			return EC_ERROR_PARAM3;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ledtest, command_ledtest,
			"<rgb led idx> <enable|disable> [color|off]", "");
