/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power/Battery LED control for Eve
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "registers.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_PWM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_PWM, format, ## args)

#define LED_TOTAL_TICKS 16
#define LED_ON_TICKS 8

static int led_debug;
static int double_tap;
static int double_tap_tick_count;
static int led_pattern;
static int ticks;

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_LEFT_LED, EC_LED_ID_RIGHT_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_WHITE,
	LED_RED_2_3,
	LED_RED_1_3,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

enum led_side {
	LED_LEFT = 0,
	LED_RIGHT,
	LED_BOTH
};

#define LED_TICKS_PER_BEAT 2
#define LED_BEATS_PER_PHASE 2
#define LED_TICKS_PER_PHASE (LED_TICKS_PER_BEAT * LED_BEATS_PER_PHASE)
#define NUM_PHASE 2
struct led_phase {
	uint8_t color[NUM_PHASE];
	uint8_t len[NUM_PHASE];
};

struct range_map {
	uint8_t max;
	uint8_t tap;
};

enum led_type {
	SOLID_GREEN = 0,
	WHITE_GREEN,
	SOLID_WHITE,
	WHITE_RED,
	SOLID_RED,
	PULSE_RED_1,
	PULSE_RED_2,
	BLINK_RED,
	OFF,
	LED_NUM_TYPES,
};

static const struct led_phase pattern[LED_NUM_TYPES] = {
	{ {LED_GREEN, LED_GREEN}, {-1, -1} },
	{ {LED_WHITE, LED_GREEN}, {2, 4} },
	{ {LED_WHITE, LED_WHITE}, {-1, -1} },
	{ {LED_WHITE, LED_RED}, {2, 4} },
	{ {LED_RED, LED_RED}, {-1, -1} },
	{ {LED_RED, LED_RED_2_3}, {4, 4} },
	{ {LED_RED, LED_RED_1_3}, {2, 4} },
	{ {LED_RED, LED_OFF}, {1, 6} },
	{ {LED_OFF, LED_OFF}, {-1, -1} },
};
/* Brightness vs. color, in the order of off, red, green and blue */
#define PWM_CHAN_PER_LED 3
static const uint8_t color_brightness[LED_COLOR_COUNT][PWM_CHAN_PER_LED] = {
	/* {Red, Green, Blue}, */
	[LED_OFF]   = {0, 0, 0},
	[LED_RED]   = {80,  0, 0},
	[LED_GREEN] = {0, 80, 0},
	[LED_BLUE] = {0, 0, 80},
	[LED_WHITE]  = {100, 100, 100},
	[LED_RED_2_3]  = {53, 0, 0},
	[LED_RED_1_3]  = {27, 0, 0},
};

static const struct range_map pattern_tbl[] = {
	{2, BLINK_RED},
	{4, PULSE_RED_2},
	{10, PULSE_RED_1},
	{15, SOLID_RED},
	{30, WHITE_RED},
	{90, SOLID_WHITE},
	{98, WHITE_GREEN},
	{100, SOLID_GREEN},
};

/**
 * Set LED color
 *
 * @param color	Enumerated color value
 * @param side		Left LED, Right LED, or both LEDs
 */
static void set_color(enum led_color color, enum led_side side)
{
	int i;
	static uint8_t saved_duty[LED_BOTH][PWM_CHAN_PER_LED];

	/* Set color for left LED */
	if (side == LED_LEFT || side == LED_BOTH) {
		for (i = 0; i < PWM_CHAN_PER_LED; i++) {
			if (saved_duty[LED_LEFT][i] !=
			    color_brightness[color][i]) {
				pwm_set_duty(PWM_CH_LED_L_RED + i,
					     100 - color_brightness[color][i]);
				saved_duty[LED_LEFT][i] =
					color_brightness[color][i];
			}
		}
	}

	/* Set color for right LED */
	if (side == LED_RIGHT || side == LED_BOTH) {
		for (i = 0; i < PWM_CHAN_PER_LED; i++) {
			if (saved_duty[LED_RIGHT][i] !=
			    color_brightness[color][i]) {
				pwm_set_duty(PWM_CH_LED_R_RED + i,
					     100 - color_brightness[color][i]);
				saved_duty[LED_RIGHT][i] =
					color_brightness[color][i];
			}
		}
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_RED] = 100;
	brightness_range[EC_LED_COLOR_BLUE] = 100;
	brightness_range[EC_LED_COLOR_GREEN] = 100;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_LEFT_LED:
		/* Set brightness for left LED */
		pwm_set_duty(PWM_CH_LED_L_RED,
			     100 - brightness[EC_LED_COLOR_RED]);
		pwm_set_duty(PWM_CH_LED_L_BLUE,
			     100 - brightness[EC_LED_COLOR_BLUE]);
		pwm_set_duty(PWM_CH_LED_L_GREEN,
			     100 - brightness[EC_LED_COLOR_GREEN]);
		break;
	case EC_LED_ID_RIGHT_LED:
		/* Set brightness for right LED */
		pwm_set_duty(PWM_CH_LED_R_RED,
			     100 - brightness[EC_LED_COLOR_RED]);
		pwm_set_duty(PWM_CH_LED_R_BLUE,
			     100 - brightness[EC_LED_COLOR_BLUE]);
		pwm_set_duty(PWM_CH_LED_R_GREEN,
			     100 - brightness[EC_LED_COLOR_GREEN]);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_register_double_tap(void)
{
	double_tap = 1;
}

static void led_manage_pattern(int side)
{
	int color;
	int phase;

	phase = ticks < LED_TICKS_PER_BEAT * pattern[led_pattern].len[0] ?
		0 : 1;
	color = pattern[led_pattern].color[phase];

	set_color(color, side);

	if (pattern[led_pattern].len[0] < 0)
		ticks = 0;
	else
		if (++ticks == LED_TICKS_PER_BEAT *
		    (pattern[led_pattern].len[0] +
		     pattern[led_pattern].len[1]))
			ticks = 0;

	if (double_tap_tick_count)
		double_tap_tick_count--;
}

static void eve_led_set_power_battery(void)
{
	enum charge_state chg_state = charge_get_state();
	int side;
	int percent_chg;
	enum led_type type;
	int tap = 0;
	int i;

	if (double_tap) {
		/* Clear double tap indication */
		if (!chipset_in_state(CHIPSET_STATE_ON))
			/* If not in S0, then set tap on */
			tap = 1;
		double_tap = 0;
	}
	/* Get active charge port which maps directly to left/right LED */
	side = charge_manager_get_active_charge_port();
	/* Ensure that side can be safely used as an index */
	if (side < 0 || side >= CONFIG_USB_PD_PORT_COUNT)
		side = LED_BOTH;

	/* Get percent charge */
	percent_chg = charge_get_percent();

	if (!double_tap_tick_count) {
		if (chg_state == PWR_STATE_CHARGE_NEAR_FULL) {
			type = SOLID_GREEN;
		} else if (chg_state == PWR_STATE_CHARGE) {
			type = SOLID_GREEN;
		} else {
			for (i = 0; i < ARRAY_SIZE(pattern_tbl); i++) {
				if (percent_chg <= pattern_tbl[i].max) {
					type = pattern_tbl[i].tap;
					break;
				}
			}
			/* Check for double tap event */
			if (tap == 0) {
				if (type <= WHITE_RED)
					type = OFF;
			} else {
				double_tap_tick_count = LED_TICKS_PER_BEAT * 8;
			}
		}

		/* If the LED pattern will change, then reset tick count and set
		 * new pattern.
		 */
		if (type != led_pattern) {
			CPRINTS("led: old = %d, new = %d", led_pattern, type);
			ticks = 0;
			led_pattern = type;
		}
	}

	led_manage_pattern(side);
}

static void led_init(void)
{
	/*
	 * Enable PWMs and set to 0% duty cycle.  If they're disabled,
	 * seems to ground the pins instead of letting them float.
	 */
	/* Initialize PWM channels for left LED */
	pwm_enable(PWM_CH_LED_L_RED, 1);
	pwm_enable(PWM_CH_LED_L_GREEN, 1);
	pwm_enable(PWM_CH_LED_L_BLUE, 1);

	/* Initialize PWM channels for right LED */
	pwm_enable(PWM_CH_LED_R_RED, 1);
	pwm_enable(PWM_CH_LED_R_GREEN, 1);
	pwm_enable(PWM_CH_LED_R_BLUE, 1);

	set_color(LED_OFF, LED_BOTH);
	led_pattern = OFF;
	ticks = 0;
	double_tap_tick_count = 0;
}
/* After pwm_pin_init() */
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_DEFAULT);

/**
 * Called by hook task every 250 ms
 */
static void led_tick(void)
{
	if (led_debug == 1)
		return;
	else if (led_debug >= 2) {
		led_manage_pattern(LED_BOTH);
		return;
	}

	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED) &&
	    led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		eve_led_set_power_battery();
		return;
	}
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

/******************************************************************/
/* Console commands */
static int command_led(int argc, char **argv)
{
	int side = LED_BOTH;
	char *e;

	if (argc > 1) {
		if (argc > 2) {
			side = strtoi(argv[2], &e, 10);
			if (*e)
				return EC_ERROR_PARAM2;
			if (side > 1)
				return EC_ERROR_PARAM2;
		}

		if (!strcasecmp(argv[1], "debug")) {
			led_debug ^= 1;
			CPRINTF("led_debug = %d\n", led_debug);
		} else if (!strcasecmp(argv[1], "off")) {
			set_color(LED_OFF, side);
		} else if (!strcasecmp(argv[1], "red")) {
			set_color(LED_RED, side);
		} else if (!strcasecmp(argv[1], "green")) {
			set_color(LED_GREEN, side);
		} else if (!strcasecmp(argv[1], "blue")) {
			set_color(LED_BLUE, side);
		} else if (!strcasecmp(argv[1], "white")) {
			set_color(LED_WHITE, side);
		} else {
			/* maybe handle charger_discharge_on_ac() too? */
			return EC_ERROR_PARAM1;
		}
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"[debug|red|green|blue|white|amber|off <0|1>]",
			"Change LED color");

/* Console commands */
static int command_led_test(int argc, char **argv)
{
	int select;
	char *e;

	if (argc > 1) {

		if (argc > 2) {
			select = strtoi(argv[2], &e, 10);
			if (*e)
				return EC_ERROR_PARAM1;
			if (select >= LED_NUM_TYPES)
				return EC_ERROR_PARAM1;

			ticks = 0;
			led_pattern = select;
			double_tap_tick_count = LED_TICKS_PER_BEAT * 8;
		}

		if (!strcasecmp(argv[1], "on")) {
			led_debug = 2;
		} else if (!strcasecmp(argv[1], "off")) {
			led_debug = 0;
		} else {
			/* maybe handle charger_discharge_on_ac() too? */
			return EC_ERROR_PARAM1;
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led_test, command_led_test,
			"[on|off 0 - 5]",
			"Change LED pattern");
