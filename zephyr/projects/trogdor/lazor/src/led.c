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

enum chipset {
	S0 = 0,
	S3,
	S5,
	ANY,
	CHIPSET_STATE_COUNT
};

struct values {
	int color_1;
	int color_2;
	int color_3;
	int period;
	int on_sec;
};

struct policy {
	bool chipset_state_enabled;
	bool chflags_enabled;
};

static struct policy led_policy[PWR_STATE_COUNT];
static struct values led_values[PWR_STATE_COUNT][CHIPSET_STATE_COUNT];

/* Lazor example
 * Battery LED color in PWR_STATE_DISCHARGE also depends on the chipset_state
 * Eg. for chipset_state S0 color is LED_BLUE, but for S3 it is blinking. So,
 * led_policy[PWR_STATE_DISCHARGE].chipset_state_enabled will be set to true.
 * However, for the rest of the states it is set to false. Similarly, chflags
 * are only used in PWR_STATE_IDLE. So
 * led_policy[PWR_STATE_IDLE].chflags_enabled is set to true and for rest of
 * the states it is set to false
 *
 * led_values describe what color to set for the combination of pwr_state and
 * chipset_state. It also describes if the leds need to blink and if so
 * what is the period (total led ticks) and color_1 and color_2. Eg.
 * led_values[PWR_STATE_DISCHARGE][S3].color_1 = LED_AMBER;
 * led_values[PWR_STATE_DISCHARGE][S3].color_2 = LED_OFF;
 * led_values[PWR_STATE_DISCHARGE][S3].period = 4;
 * led_values[PWR_STATE_DISCHARGE][S3].on_sec = 1;
 * So 1 sec amber, 3 sec off.
 * For pwr_state where led isn't blinking and also doesn't depend on chipset
 * state:
 * led_values[PWR_STATE_CHARGE][ANY].color_1 = LED_AMBER;
 * led_values[PWR_STATE_CHARGE][ANY].period = 0;
 * led_values[PWR_STATE_CHARGE][ANY].on_sec = 0;
 */

#define CHARGE_STATE(id)  DT_STRING_UPPER_TOKEN(id, charge_state)
#define CHIPSET_STATE(id) DT_STRING_UPPER_TOKEN(id, chipset_state)
#define COLOR(id, color)  DT_STRING_UPPER_TOKEN(DT_CHILD(id, color), led_color)

#define SET_LED_POLICY_AND_VALUES(id)					       \
	do {								       \
		if (DT_PROP(id, chflags))				       \
			led_policy[CHARGE_STATE(id)].chflags_enabled = true;   \
		else							       \
			led_policy[CHARGE_STATE(id)].chflags_enabled = false;  \
		if (CHIPSET_STATE(id) != ANY)				       \
			led_policy[CHARGE_STATE(id)].chipset_state_enabled     \
								   = true;     \
		else							       \
			led_policy[CHARGE_STATE(id)].chipset_state_enabled     \
								   = false;    \
		led_values[CHARGE_STATE(id)][CHIPSET_STATE(id)].period =       \
			DT_PROP(id, period);				       \
		led_values[CHARGE_STATE(id)][CHIPSET_STATE(id)].on_sec =       \
			DT_PROP(id, on_sec);				       \
		led_values[CHARGE_STATE(id)][CHIPSET_STATE(id)].color_1 =      \
			COLOR(id, color_1);				       \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(id, color_2)),	       \
			(led_values[CHARGE_STATE(id)][CHIPSET_STATE(id)].      \
			 color_2 = COLOR(id, color_2);))		       \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(id, color_3)),	       \
			(led_values[CHARGE_STATE(id)][CHIPSET_STATE(id)].      \
			 color_3 = COLOR(id, color_3);))		       \
	} while (0);

static void led_init(void)
{
#if DT_NODE_EXISTS(BATT_LED_NODE)
	DT_FOREACH_CHILD(BATT_LED_NODE, SET_LED_POLICY_AND_VALUES)
#endif
}
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_DEFAULT);

static int get_led_color(int battery_ticks, enum charge_state pwr_state,
						enum chipset chipset_state)
{
	int color = LED_OFF;
	int on_sec = led_values[pwr_state][chipset_state].on_sec;

	if (battery_ticks < on_sec)
		color = led_values[pwr_state][chipset_state].color_1;
	else
		color = led_values[pwr_state][chipset_state].color_2;

	return color;
}

static int board_led_set_battery(void)
{
	static int battery_ticks;
	int color = LED_OFF;
	int period = 0;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	enum charge_state pwr_state = charge_get_state();

	/* Only PWR_STATE_IDLE depends on chflags */
	if (led_policy[pwr_state].chflags_enabled) {
		if (chflags & CHARGE_FLAG_FORCE_IDLE) {
			period = led_values[pwr_state][ANY].period;
			color = get_led_color((battery_ticks % period),
							pwr_state, ANY);
		} else {
			color = led_values[pwr_state][ANY].color_3;
		}
	/* LED Color depends on chipset_state */
	} else if (led_policy[pwr_state].chipset_state_enabled) {
		enum chipset chipset_state;

		if (chipset_in_state(CHIPSET_STATE_ON))
			/* S0 */
			chipset_state = S0;
		else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			/* S3 */
			chipset_state = S3;
		else if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			/* S5 */
			chipset_state = S5;

		period = led_values[pwr_state][chipset_state].period;

		if (period != 0)
			color = get_led_color((battery_ticks % period),
						pwr_state, chipset_state);
		else
			color = led_values[pwr_state][chipset_state].color_1;
	/* LED is blinking in this state */
	} else if (led_values[pwr_state][ANY].period != 0) {
		period = led_values[pwr_state][ANY].period;
		color = get_led_color((battery_ticks % period),
						pwr_state, ANY);
	} else
		color = led_values[pwr_state][ANY].color_1;

	return color;
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
