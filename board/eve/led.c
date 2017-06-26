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

#define LED_TICKS_PER_BEAT 2
#define NUM_PHASE 2
#define DOUBLE_TAP_TICK_LEN (LED_TICKS_PER_BEAT * 8)
#define LED_FRAC_BITS 4
#define LED_STEP_MSEC 40

static int led_debug;
static int double_tap;
static int double_tap_tick_count;
static int led_pattern;
static int led_ticks;
static enum led_color led_current_color;

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_LEFT_LED, EC_LED_ID_RIGHT_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

/* List of LED colors used */
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

/* List of supported LED patterns */
enum led_pattern {
	SOLID_GREEN = 0,
	WHITE_GREEN,
	SOLID_WHITE,
	WHITE_RED,
	SOLID_RED,
	PULSE_RED_1,
	PULSE_RED_2,
	BLINK_RED,
	OFF,
	LED_NUM_PATTERNS,
};

enum led_side {
	LED_LEFT = 0,
	LED_RIGHT,
	LED_BOTH
};

/*
 * LED patterns are described as two phases. Each phase has an associated LED
 * color and length in beats. The length of each beat is defined by the macro
 * LED_TICKS_PER_BEAT.
 */
struct led_phase {
	uint8_t color[NUM_PHASE];
	uint8_t len[NUM_PHASE];
};

/*
 * Pattern table. The len field is beats per color. 0 for len indicates that a
 * particular pattern never changes from the first phase.
 */
static const struct led_phase pattern[LED_NUM_PATTERNS] = {
	{ {LED_GREEN, LED_GREEN}, {0, 0} },
	{ {LED_WHITE, LED_GREEN}, {2, 4} },
	{ {LED_WHITE, LED_WHITE}, {0, 0} },
	{ {LED_WHITE, LED_RED}, {2, 4} },
	{ {LED_RED, LED_RED}, {0, 0} },
	{ {LED_RED, LED_RED_2_3}, {4, 4} },
	{ {LED_RED, LED_RED_1_3}, {2, 4} },
	{ {LED_RED, LED_OFF}, {1, 6} },
	{ {LED_OFF, LED_OFF}, {0, 0} },
};

/*
 * Brightness vs. color, in the order of off, red, green and blue. Values are
 * for % on PWM duty cycle time.
 */
#define PWM_CHAN_PER_LED 3
static const uint8_t color_brightness[LED_COLOR_COUNT][PWM_CHAN_PER_LED] = {
	/* {Red, Green, Blue}, */
	[LED_OFF]   = {0, 0, 0},
	[LED_RED]   = {80,  0, 0},
	[LED_GREEN] = {0, 80, 0},
	[LED_BLUE] = {0, 0, 80},
	[LED_WHITE]  = {100, 100, 100},
	[LED_RED_2_3]  = {40, 0, 0},
	[LED_RED_1_3]  = {20, 0, 0},
};

/*
 * When a double tap event occurs, a LED pattern is displayed based on the
 * current battery charge level. The LED patterns used for double tap under low
 * battery conditions are same patterns displayed when the battery is not
 * charging. The table below shows what battery charge level displays which
 * pattern.
 */
struct range_map {
	uint8_t max;
	uint8_t pattern;
};

#if (CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC >= 3)
#error "LED: PULSE_RED_2 battery level <= BLINK_RED level"
#endif
static const struct range_map pattern_tbl[] = {
	{CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC - 1, BLINK_RED},
	{3, PULSE_RED_2},
	{9, PULSE_RED_1},
	{14, SOLID_RED},
	{29, WHITE_RED},
	{89, SOLID_WHITE},
	{97, WHITE_GREEN},
	{100, SOLID_GREEN},
};

enum led_change {
	led_intensity_down,
	led_intensity_up,
	led_done,
};

/*
 * The PWM % on levels to transition from intensity 0 (black) to intensity 1.0
 * (white) in the HSI color space converted back to RGB space (0 - 255) and
 * converted to a % for PWM. This table is used for Red <--> White and Green
 * <--> Transitions. In HSI space white = (0, 0, 1), red = (0, .5, .33), green =
 * (120, .5, .33). For the transitions of interest only S and I are changed and
 * they are changed linearly in HSI space.
 */
static const uint8_t trans_steps[] = {0, 5, 13, 22, 33, 47, 63, 80, 100};

/**
 * Set LED color
 *
 * @param pwm		Pointer to 3 element RGB color level (0 -> 100)
 * @param side		Left LED, Right LED, or both LEDs
 */
static void set_color(const uint8_t *pwm, enum led_side side)
{
	int i;
	static uint8_t saved_duty[LED_BOTH][PWM_CHAN_PER_LED];

	/* Set color for left LED */
	if (side == LED_LEFT || side == LED_BOTH) {
		for (i = 0; i < PWM_CHAN_PER_LED; i++) {
			if (saved_duty[LED_LEFT][i] != pwm[i]) {
				pwm_set_duty(PWM_CH_LED_L_RED + i,
					     100 - pwm[i]);
				saved_duty[LED_LEFT][i] = pwm[i];
			}
		}
	}

	/* Set color for right LED */
	if (side == LED_RIGHT || side == LED_BOTH) {
		for (i = 0; i < PWM_CHAN_PER_LED; i++) {
			if (saved_duty[LED_RIGHT][i] != pwm[i]) {
				pwm_set_duty(PWM_CH_LED_R_RED + i,
					     100 - pwm[i]);
				saved_duty[LED_RIGHT][i] = pwm[i];
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

static void led_change_color(int old_idx, int new_idx, enum led_side side)
{
	int i;
	int step;
	int state;
	uint8_t rgb_current[PWM_CHAN_PER_LED];
	uint8_t rgb_target[PWM_CHAN_PER_LED];
	uint8_t trans[ARRAY_SIZE(trans_steps)];
	int rgb_mask = 0;

	/*
	 * Using the color indices, poplulate the current and target R, G, B
	 * arrays. The arrays are indexed R = 0, G = 1, B = 2. If the target of
	 * any of the 3 is greater than the current, then this color change is
	 * an increase in intensity. Otherwise, it's a decrease.
	 */
	for (i = 0; i < PWM_CHAN_PER_LED; i++) {
		rgb_current[i] = color_brightness[old_idx][i];
		rgb_target[i] = color_brightness[new_idx][i];
		if (rgb_current[i] < rgb_target[i]) {
			/* increase in color */
			rgb_mask |= 1;
		}
	}
	/* Check to see if increasing or decreasing color */
	if (rgb_mask) {
		/* First entry of transition table == current level */
		step = 1;
		state = led_intensity_up;
	} else {
		/* Last entry of transition table == current level */
		step = ARRAY_SIZE(trans_steps) - 2;
		state = led_intensity_down;
	}

	/*
	 * Populate transition table based on the number of R, G, B components
	 * changing. If only 1 componenet is changing, then can just do linear
	 * steps over the range. If more than 1 component is changing, then
	 * this is a white <--> color transition and will use
	 * the precomputed steps which are derived by converting to HSI space
	 * and then linearly transitioning S and I to go from the starting color
	 * to white and vice versa.
	 */
	if (old_idx == LED_WHITE || new_idx == LED_WHITE) {
		for (i = 0; i < ARRAY_SIZE(trans_steps); i++)
			trans[i] = trans_steps[i];
	} else {
		int delta_per_step;
		int step_value;
		int start_lvl;
		int total_change;
		int color_index = 0;

		/*
		 * Since the new or old color is not white, then this change
		 * must involve only either red or green. There are no red <-->
		 * green transitions. So only 1 color is being changed in this
		 * case. Assume it's red (index = 0), but check if it's green
		 * (index = 1).
		 */

		if (old_idx == LED_GREEN || new_idx == LED_GREEN)
			color_index = 1;

		/*
		 * Determine the total change assuming current level is higher
		 * than target level. The transitions steps are always ordered
		 * lower to higher. The starting index is adjusted if intensity
		 * is decreasing.
		 */
		total_change = rgb_current[color_index] -
			rgb_target[color_index];
		start_lvl = rgb_target[color_index];

		if (total_change < 0) {
			/*
			 * Increasing in intensity, current level or R/G is
			 * the starting level.
			 */
			total_change = -total_change;
			start_lvl = rgb_current[color_index];
		}
		/*
		 * Compute change per step using fractional bits. The step
		 * change accumulates fractional bits and is truncated after
		 * rounding before being added to the starting value.
		 */
		delta_per_step = (total_change << LED_FRAC_BITS)
			/ (ARRAY_SIZE(trans_steps) - 1);
		step_value = 0;
		for (i = 0; i < ARRAY_SIZE(trans_steps); i++) {
			trans[i] = start_lvl +
				((step_value +
				  (1 << (LED_FRAC_BITS - 1)))
				 >> LED_FRAC_BITS);
			step_value += delta_per_step;
		}
	}

	/* Will loop here until the color change is complete. */
	while (state != led_done) {
		rgb_mask = 0;
		if (state == led_intensity_down) {
			/*
			 * Colors are going from higher to lower level. If the
			 * current level of R, G, or B is higher than both
			 * the next step in the transition table and and the
			 * target level, then move to the larger of the two. The
			 * MAX is used to make sure that it doens't drop below
			 * the target level.
			 */
			for (i = 0; i < PWM_CHAN_PER_LED; i++) {
				if ((rgb_current[i] > rgb_target[i]) &&
				    (rgb_current[i] >= trans[step])) {
					rgb_current[i] = MAX(trans[step],
							     rgb_target[i]);
					rgb_mask |= 1;
				}
			}
			/*
			 * If nothing changed this iteration, or if lowest table
			 * entry has been used, then the change is complete.
			 */
			if (!rgb_mask || --step < 0)
				state = led_done;

		} else if (state == led_intensity_up) {
			/*
			 * Colors are going from lower to higher level. If the
			 * current level of R, G, B is lower than both the
			 * target level and the transition table entry for a
			 * given color, then move up to the MIN of next
			 * transition step and target level.
			 */
			for (i = 0; i < PWM_CHAN_PER_LED; i++) {
				if ((rgb_current[i] < rgb_target[i]) &&
				    (rgb_current[i] <= trans[step])) {
					rgb_current[i] = MIN(trans[step],
							     rgb_target[i]);
					rgb_mask |= 1;
				}
			}
			/*
			 * If nothing changed this iteration, or if highest
			 * table entry has been used, then the change is
			 * complete.
			 */
			if (!rgb_mask || ++step >= ARRAY_SIZE(trans_steps))
				state = led_done;
		}
		/* Apply current R, G, B levels */
		set_color(rgb_current, side);
		msleep(LED_STEP_MSEC);
	}
}

static void led_manage_pattern(int side)
{
	int color;
	int phase;

	/* Determine pattern phase */
	phase = led_ticks < LED_TICKS_PER_BEAT * pattern[led_pattern].len[0] ?
		0 : 1;
	color = pattern[led_pattern].color[phase];
	/* If color is changing, then manage the transition */
	if (led_current_color != color) {
		led_change_color(led_current_color, color, side);
		led_current_color = color;
	}
	/* Set color for the current phase */
	set_color(color_brightness[color], side);

	/*
	 * Update led_ticks. If the len field is 0, then the pattern
	 * being used is just one color so no need to increase the tick count.
	 */
	if (pattern[led_pattern].len[0])
		if (++led_ticks == LED_TICKS_PER_BEAT *
		    (pattern[led_pattern].len[0] +
		     pattern[led_pattern].len[1]))
			led_ticks = 0;

	/* If double tap display is active, decrement its counter */
	if (double_tap_tick_count)
		double_tap_tick_count--;
}

static void eve_led_set_power_battery(void)
{
	enum charge_state chg_state = charge_get_state();
	int side;
	int percent_chg;
	enum led_pattern pattern = led_pattern;
	int tap = 0;

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

	if (chg_state == PWR_STATE_CHARGE_NEAR_FULL ||
	    ((chg_state == PWR_STATE_DISCHARGE_FULL)
	     && extpower_is_present())) {
		pattern = SOLID_GREEN;
		double_tap_tick_count = 0;
	} else if (chg_state == PWR_STATE_CHARGE) {
		pattern = SOLID_WHITE;
		double_tap_tick_count = 0;
	} else if (!double_tap_tick_count) {
		int i;

		/*
		 * Not currently charging. Select the pattern based on
		 * the battery charge level. If there is no double tap
		 * event to process, then only the low battery patterns
		 * are relevant.
		 */
		for (i = 0; i < ARRAY_SIZE(pattern_tbl); i++) {
			if (percent_chg <= pattern_tbl[i].max) {
				pattern = pattern_tbl[i].pattern;
				break;
			}
		}
		/*
		 * The patterns used for double tap and for not charging
		 * state are the same for low battery cases. But, if
		 * battery charge is high enough to be above SOLID_RED,
		 * then only display LED pattern if double tap has
		 * occurred.
		 */
		if (tap == 0 && pattern <= WHITE_RED)
			pattern = OFF;
		else
			/* Start double tap LED sequence */
			double_tap_tick_count = DOUBLE_TAP_TICK_LEN;
	}

	/* If the LED pattern will change, then reset tick count and set
	 * new pattern.
	 */
	if (pattern != led_pattern) {
		led_ticks = 0;
		led_pattern = pattern;
	}
	/*
	 * If external charger is connected, then make sure only the LED that's
	 * on the side with the charger is turned on.
	 */
	if (side != LED_BOTH)
		set_color(color_brightness[LED_OFF], side ^ 1);
	/* Update LED pattern */
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

	set_color(color_brightness[LED_OFF], LED_BOTH);
	led_pattern = OFF;
	led_ticks = 0;
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

	if (led_auto_control_is_enabled(EC_LED_ID_LEFT_LED) &&
	    led_auto_control_is_enabled(EC_LED_ID_RIGHT_LED)) {
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
	int color;

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
			return EC_SUCCESS;
		}

		if (!strcasecmp(argv[1], "off"))
			color = LED_OFF;
		else if (!strcasecmp(argv[1], "red"))
			color = LED_RED;
		else if (!strcasecmp(argv[1], "green"))
			color = LED_GREEN;
		else if (!strcasecmp(argv[1], "blue"))
			color = LED_BLUE;
		else if (!strcasecmp(argv[1], "white"))
			color = LED_WHITE;
		else
			return EC_ERROR_PARAM1;

		set_color(color_brightness[color], side);
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"[debug|red|green|blue|white|amber|off <0|1>]",
			"Change LED color");
