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

#define GPIO_LED_NODE    DT_PATH(gpio_led, gpio_led_colors)
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
	struct led_color_t led_colors[MAX_COLOR];
};

/* acc_period is the accumulated period value of all color-x children
 * led_colors[0].acc_period = period value of color-0 node
 * led_colors[1].acc_period = period value of color-0 + color-1 nodes
 * led_colors[2].acc_period = period value of color-0 + color-1 + color-2 nodes
 * and so on. If period prop or color node doesn't exist, period val is 0
 */

#define PERIOD_VAL(id) COND_CODE_1(DT_NODE_HAS_PROP(id, period),	\
				   (DT_PROP(id, period)),		\
				   (0))

#define LED_PERIOD(color_num, state_id)					\
	PERIOD_VAL(DT_CHILD(state_id, color_##color_num))

#define LED_PLUS_PERIOD(color_num, state_id)				\
	+ LED_PERIOD(color_num, state_id)

#define ACC_PERIOD(color_num, state_id)					\
	(0 UTIL_LISTIFY(color_num, LED_PLUS_PERIOD, state_id))

#define GET_PROP(id, prop)						\
	COND_CODE_1(DT_NODE_HAS_PROP(id, prop),				\
		    (DT_STRING_UPPER_TOKEN(id, prop)),			\
		    (0))

#define LED_COLOR_INIT(color_num, color_num_plus_one, state_id)		\
{									\
	.led_color = GET_PROP(DT_CHILD(state_id, color_##color_num),	\
							led_color),	\
	.acc_period = ACC_PERIOD(color_num_plus_one, state_id)		\
}

/* Initialize node_array struct with prop listed in dts */
#define SET_LED_VALUES(state_id)					\
{									\
	.pwr_state = GET_PROP(state_id, charge_state),			\
	.chipset_state = GET_PROP(state_id, chipset_state),		\
	.extra_flag = GET_PROP(state_id, extra_flag),			\
	.led_colors = {LED_COLOR_INIT(0, 1, state_id),			\
		       LED_COLOR_INIT(1, 2, state_id),			\
		       LED_COLOR_INIT(2, 3, state_id),			\
		       LED_COLOR_INIT(3, 4, state_id),			\
		      }							\
},

struct node_prop_t node_array[] = {
	DT_FOREACH_CHILD(GPIO_LED_NODE, SET_LED_VALUES)
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

static bool find_node_with_extra_flag(int i)
{
	uint32_t chflags = charge_get_flags();
	bool found_node = false;

	switch (node_array[i].extra_flag) {
	case CHFLAG_FORCE_IDLE:
	case CHFLAG_DEFAULT:
		if (chflags & CHARGE_FLAG_FORCE_IDLE) {
			if (node_array[i].extra_flag ==	CHFLAG_FORCE_IDLE)
				found_node = true;
		} else {
			if (node_array[i].extra_flag ==	CHFLAG_DEFAULT)
				found_node = true;
		}
		break;
	case BATT_BELOW_10:
	case BATT_ABOVE_10:
		if (charge_get_percent() < 10) {
			if (node_array[i].extra_flag ==	BATT_BELOW_10)
				found_node = true;
		} else {
			if (node_array[i].extra_flag !=	BATT_ABOVE_10)
				found_node = true;
		}
		break;
	default:
		break;
	}

	return found_node;
}

static int find_node(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(node_array); i++) {
		/* Check if this node depends on power state */
		if (node_array[i].pwr_state != PWR_STATE_UNCHANGE) {
			enum charge_state pwr_state = charge_get_state();

			if (node_array[i].pwr_state != pwr_state)
				continue;
		}

		/* Check if this node depends on chipset state */
		if (node_array[i].chipset_state != ANY) {
			enum chipset_state_t chipset_state =
							get_chipset_state();

			/* Continue at current index as nodes are in sequence */
			if (node_array[i].chipset_state != chipset_state)
				continue;
		}

		/* Check if the node depends on any special flags */
		if (node_array[i].extra_flag != NONE)
			if (!find_node_with_extra_flag(i))
				continue;

		/* We found the node */
		break;
	}

	return i;
}

#define GET_PERIOD(n_idx, c_idx)  node_array[n_idx].led_colors[c_idx].acc_period
#define GET_COLOR(n_idx, c_idx)   node_array[n_idx].led_colors[c_idx].led_color

static int find_color(int node_idx, int ticks)
{
	int color_idx = 0;

	/* If period value at index 0 is not 0, it's a blinking LED */
	if (GET_PERIOD(node_idx, 0) != 0) {
		/*  Period is accumulated at the last index */
		ticks = ticks % GET_PERIOD(node_idx, MAX_COLOR - 1);

		for (color_idx = 0; color_idx < MAX_COLOR; color_idx++) {
			if (GET_PERIOD(node_idx, color_idx) < ticks)
				break;
		}
	}

	return GET_COLOR(node_idx, color_idx);
}

static void board_led_set_color(void)
{
	int color = LED_OFF;
	int node = 0;
	static int ticks;

	ticks++;

	node = find_node();
	color = find_color(node, ticks);

	led_set_color(color);
}

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		board_led_set_color();
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
		board_led_set_color();
		return;
	}

	color = state ? DT_STRING_UPPER_TOKEN(LED_CONTROL, led_color) : LED_OFF;

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);

	led_set_color(color);
}
