/* Copyright 2021 The Chromium OS Authors. All rights reserved.
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
#include "system.h"
#include "util.h"

#include <devicetree.h>

#define LED_ONE_SEC (1000 / HOOK_TICK_INTERVAL_MS)

#define BAT_LED_ON 1
#define BAT_LED_OFF 0

#define BATT_LED_NODE    DT_PATH(gpio_led, battery_led_colors)
#define LED_CONTROL      DT_NODELABEL(led_control)

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_BLUE,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static void led_set_color(enum led_color color)
{
	gpio_set_level(GPIO_EC_CHG_LED_Y_C1,
		(color == LED_AMBER) ? BAT_LED_ON : BAT_LED_OFF);
	gpio_set_level(GPIO_EC_CHG_LED_B_C1,
		(color == LED_BLUE) ? BAT_LED_ON : BAT_LED_OFF);
}

static const uint8_t dt_brigthness_range[EC_LED_COLOR_COUNT] =
	DT_PROP(DT_PATH(gpio_led, brightness_range), brightness_range_battery);

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	memcpy(brightness_range, dt_brigthness_range,
		sizeof(dt_brigthness_range));
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_BLUE] != 0)
		led_set_color(LED_BLUE);
	else if (brightness[EC_LED_COLOR_AMBER] != 0)
		led_set_color(LED_AMBER);
	else
		led_set_color(LED_OFF);

	return EC_SUCCESS;
}

struct led_color_t {
	int led_color;
	int acc_period;
};

enum extra_flag_t {
	NONE = 0,
	CHFLAG_FORCE_IDLE,
	CHFLAG_DEFAULT,
	BATT_BELOW_10,
	BATT_ABOVE_10,
};

enum chipset_state_t {
	ANY = 0,
	S0,
	S3,
	S5,
};

#define MAX_COLOR	4

struct node_prop_t {
	enum charge_state pwr_state;
	enum chipset_state_t chipset_state;
	enum extra_flag_t extra_flag;
	struct led_color_t led_color[MAX_COLOR];
};

#define GET_PROP(id, prop) COND_CODE_1(DT_NODE_HAS_PROP(id, prop),	  \
				       (DT_STRING_UPPER_TOKEN(id, prop)), \
				       (0))

/* TODO: Fix setting led_color values */
#define SET_LED_VALUES(id)						  \
	{								  \
		.pwr_state = GET_PROP(id, charge_state),		  \
		.chipset_state = GET_PROP(id, chipset_state),		  \
		.extra_flag = GET_PROP(id, extra_flag),			  \
		.led_color = {{0, 0},					  \
			      {0, 1},					  \
			      {0, 1},					  \
			      {0, 1}					  \
			     }						  \
	},

struct node_prop_t node_array[] = {
	DT_FOREACH_CHILD(BATT_LED_NODE, SET_LED_VALUES)
};

static enum chipset_state_t get_chipset_state(void)
{
	enum chipset_state_t chipset_state;

	if (chipset_in_state(CHIPSET_STATE_ON))
		/* S0 */
		chipset_state = S0;
	else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
		/* S3 */
		chipset_state = S3;
	else if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		/* S5 */
		chipset_state = S5;

	return chipset_state;
}

static int find_node(enum charge_state pwr_state)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(node_array); i++) {
		if (node_array[i].pwr_state == pwr_state)
			break;
	}

	/* Check if this node depends on chipset state */
	if (node_array[i].chipset_state != ANY) {
		enum chipset_state_t chipset_state = get_chipset_state();

		/* Continue at current index as nodes are in sequence */
		while (node_array[i].chipset_state != chipset_state)
			i++;
	}

	/* Check if the node depends on any special flags */
	if (node_array[i].extra_flag != NONE) {
		uint32_t chflags = charge_get_flags();

		switch (node_array[i].extra_flag) {
		case CHFLAG_FORCE_IDLE:
		case CHFLAG_DEFAULT:
			if (chflags & CHARGE_FLAG_FORCE_IDLE) {
				while (node_array[i].extra_flag !=
							CHFLAG_FORCE_IDLE)
					i++;
			} else {
				while (node_array[i].extra_flag !=
							CHFLAG_DEFAULT)
					i++;
			}
			break;
		case BATT_BELOW_10:
		case BATT_ABOVE_10:
			if (charge_get_percent() < 10) {
				while (node_array[i].extra_flag !=
							BATT_BELOW_10)
					i++;
			} else {
				while (node_array[i].extra_flag !=
							BATT_ABOVE_10)
					i++;
			}
			break;
		default:
			break;
		}
	}

	return i;
}

#define GET_PERIOD(n_idx, c_idx)  node_array[n_idx].led_color[c_idx].acc_period
#define GET_COLOR(n_idx, c_idx)   node_array[n_idx].led_color[c_idx].led_color

static int find_color(int node_idx, int ticks)
{
	int color_idx = 0;

	/* If period value at index 0 is not 0, it's a blinking LED */
	if (node_array[node_idx].led_color[0].acc_period != 0) {
		ticks = ticks % GET_PERIOD(node_idx, MAX_COLOR - 1);

		for (color_idx = 0; color_idx < MAX_COLOR; color_idx++) {
			if (GET_PERIOD(node_idx, color_idx) < ticks)
				break;
		}
	}

	return GET_COLOR(node_idx, color_idx);
}

static void board_led_set_battery(void)
{
	int color = LED_OFF;
	int node = 0;
	enum charge_state pwr_state = charge_get_state();
	static int ticks;

	ticks++;

	node = find_node(pwr_state);
	color = find_color(node, ticks);

	led_set_color(color);
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		board_led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

void led_control(enum ec_led_id led_id, enum ec_led_state state)
{
	enum led_color color;

	if ((led_id != EC_LED_ID_RECOVERY_HW_REINIT_LED) &&
	    (led_id != EC_LED_ID_SYSRQ_DEBUG_LED))
		return;

	if (state == LED_STATE_RESET) {
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		board_led_set_battery();
		return;
	}

	color = state ? DT_STRING_UPPER_TOKEN(LED_CONTROL, led_color) : LED_OFF;

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);

	led_set_color(color);
}
