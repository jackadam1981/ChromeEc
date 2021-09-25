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

#define LED_ON_LVL  1
#define LED_OFF_LVL 0

#define BRIGHTNESS_RANGE DT_NODELABEL(brightness_range)

/* Battery LED SM Defines */
#define CHARGING        DT_NODELABEL(charging)
#define DISCHARGE_S0    DT_NODELABEL(discharge_s0)
#define DISCHARGE_S3    DT_NODELABEL(discharge_s3)
#define DISCHARGE_S5    DT_NODELABEL(discharge_s5)
#define ERROR           DT_NODELABEL(error)
#define IDLE            DT_NODELABEL(idle)
#define LED_CONTROL     DT_NODELABEL(led_control)
#define LED_ID_BATT     DT_PATH(gpio_led, battery_led_colors)
#define NEAR_FULL       DT_NODELABEL(near_full)

/* Power LED SM Defines */
#define LED_ID_POWER    DT_PATH(gpio_led, power_led_colors)
#define POWER_LED_NODE  DT_NODE_EXISTS(LED_ID_POWER)
#define POWER_STATE_S0  DT_NODELABEL(power_state_s0)
#define POWER_STATE_S3  DT_NODELABEL(power_state_s3)
#define POWER_STATE_S5  DT_NODELABEL(power_state_s5)

#define LED_ENUM(id) DT_ENUM_UPPER_TOKEN(id, enum_name)
#define LED_ENUM_WITH_COMMA(id) \
	COND_CODE_1(DT_NODE_HAS_PROP(id, enum_name), (LED_ENUM(id), ), ())

const enum ec_led_id supported_led_ids[] = {
#if DT_NODE_EXISTS(DT_PATH(gpio_led, led_id))
	DT_FOREACH_CHILD(DT_PATH(gpio_led, led_id), LED_ENUM_WITH_COMMA)
#endif
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_YELLOW,
	LED_WHITE,
	LED_AMBER,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static void led_set_color(enum led_color color)
{
	gpio_set_level(GPIO_EC_CHG_LED_Y_C1,
		(color == LED_AMBER) ? LED_ON_LVL : LED_OFF_LVL);
	gpio_set_level(GPIO_EC_CHG_LED_B_C1,
		(color == LED_BLUE) ? LED_ON_LVL : LED_OFF_LVL);
}

#if POWER_LED_NODE
static void led_set_color_power(enum led_color color)
{
	gpio_set_level(GPIO_EC_PWR_LED_W,
		(color == LED_WHITE) ? LED_ON_LVL : LED_OFF_LVL);
}
#endif

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		const uint8_t dt_brigthness_range[EC_LED_COLOR_COUNT] =
			DT_PROP(BRIGHTNESS_RANGE, brightness_range_battery);

		memcpy(brightness_range, dt_brigthness_range,
			sizeof(dt_brigthness_range));
	}
#if POWER_LED_NODE
	else if (led_id == EC_LED_ID_POWER_LED) {
		const uint8_t dt_brigthness_range[EC_LED_COLOR_COUNT] =
			DT_PROP(BRIGHTNESS_RANGE, brightness_range_power);

		memcpy(brightness_range, dt_brigthness_range,
			sizeof(dt_brigthness_range));
	}
#endif
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

static void board_led_set_battery(void)
{
	static int battery_ticks;
	int color = LED_OFF;
	int period = 0;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		color = DT_ENUM_TOKEN(CHARGING, led_color);
		break;
	case PWR_STATE_DISCHARGE:
		/* TODO: Need additional work to make it work for Coachz */
#if DT_PROP(LED_ID_BATT, chipset_state)
		if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
			/* Discharging in S3 */
			period = DT_PROP(DISCHARGE_S3, period);
			battery_ticks = battery_ticks % period;
			if (battery_ticks < 1 * LED_ONE_SEC)
				color = DT_ENUM_TOKEN(
						DISCHARGE_S3, led_color_1);
			else
				color = DT_ENUM_TOKEN(
						DISCHARGE_S3, led_color_2);
		} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			/* Discharging in S5 */
			color = DT_ENUM_TOKEN(DISCHARGE_S5, led_color);
		} else if (chipset_in_state(CHIPSET_STATE_ON)) {
			/* Discharging in S0 */
			color = DT_ENUM_TOKEN(DISCHARGE_S0, led_color);
		}
#else
		color = LED_OFF;
#endif
		break;
	case PWR_STATE_ERROR:
		/* Battery error */
		period = DT_PROP(ERROR, period);
		battery_ticks = battery_ticks % period;
		if (battery_ticks < 1 * LED_ONE_SEC)
			color = DT_ENUM_TOKEN(ERROR, led_color_1);
		else
			color = DT_ENUM_TOKEN(ERROR, led_color_2);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		/* Full Charged */
		/* TODO: Need additional logic for chipset states */
		color = DT_ENUM_TOKEN(NEAR_FULL, led_color);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		if (chflags & CHARGE_FLAG_FORCE_IDLE) {
			/* Factory mode */
			period = DT_PROP(IDLE, period);
			battery_ticks = battery_ticks % period;
			if (battery_ticks < 2 * LED_ONE_SEC)
				color = DT_ENUM_TOKEN(IDLE, led_color_1);
			else
				color = DT_ENUM_TOKEN(IDLE, led_color_2);
		} else
			color = DT_ENUM_TOKEN(IDLE, led_color_3);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}

	led_set_color(color);
}

#if POWER_LED_NODE
static void board_led_set_power(void)
{
	static int power_ticks;
	int color = LED_OFF;
	int period = 0;

	power_ticks++;

	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
		/* S3 */
		period = DT_PROP(POWER_STATE_S3, period);
		power_ticks = power_ticks % period;
		if (power_ticks < 1)
			color = DT_ENUM_TOKEN(POWER_STATE_S3, led_color_1);
		else
			color = DT_ENUM_TOKEN(POWER_STATE_S3, led_color_2);
	} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		/* S5: LED off */
		color = DT_ENUM_TOKEN(POWER_STATE_S5, led_color);
	} else if (chipset_in_state(CHIPSET_STATE_ON)) {
		/* S0: LED on */
		color = DT_ENUM_TOKEN(POWER_STATE_S0, led_color);
	}

	led_set_color_power(color);
}
#endif

/* Called by hook task every TICK */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		board_led_set_battery();
#if POWER_LED_NODE
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		board_led_set_power();
#endif
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

	color = state ? DT_ENUM_TOKEN(LED_CONTROL, led_color) : LED_OFF;

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);

	led_set_color(color);
}
