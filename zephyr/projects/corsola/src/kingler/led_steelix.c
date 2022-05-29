/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Steelix
 */

#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "board_led.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "hooks.h"
#include "led_common.h"
#include "power.h"
#include "util.h"

LOG_MODULE_REGISTER(board_led, LOG_LEVEL_ERR);

#define BOARD_LED_PWM_PERIOD_NS BOARD_LED_HZ_TO_PERIOD_NS(100)

static const struct board_led_pwm_dt_channel board_led_battery_red =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_battery_red));
static const struct board_led_pwm_dt_channel board_led_battery_green =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_battery_green));
static const struct board_led_pwm_dt_channel board_led_battery_blue =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_battery_blue));
static const struct board_led_pwm_dt_channel board_led_power_white =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_power_white));

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum ec_led_color {
	LED_OFF = 0,
	LED_COLOR_RED,
	LED_COLOR_GREEN,
	LED_COLOR_BLUE,
	LED_COLOR_WHITE,
	LED_COLOR_YELLOW,
	LED_COLOR_PINK,

	ELED_COLOR_COUNT
};

static void board_led_pwm_set_duty(const struct board_led_pwm_dt_channel *ch,
				   int percent)
{
	uint32_t pulse_ns;
	int rv;

	if (!device_is_ready(ch->dev)) {
		LOG_ERR("PWM device %s not ready", ch->dev->name);
		return;
	}

	pulse_ns = DIV_ROUND_NEAREST(BOARD_LED_PWM_PERIOD_NS * percent, 100);

	LOG_DBG("Board LED PWM %s set percent (%d), pulse %d",
		ch->dev->name, percent, pulse_ns);

	rv = pwm_set(ch->dev, ch->channel, BOARD_LED_PWM_PERIOD_NS, pulse_ns,
		     ch->flags);
	if (rv) {
		LOG_ERR("pwm_set() failed %s (%d)", ch->dev->name, rv);
	}
}

void led_set_color_battery(enum ec_led_color color)
{
	switch (color) {
	case LED_COLOR_RED:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	case LED_COLOR_GREEN:
		board_led_pwm_set_duty(&board_led_battery_red, 0);
		board_led_pwm_set_duty(&board_led_battery_green, 100);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	case LED_COLOR_BLUE:
		board_led_pwm_set_duty(&board_led_battery_red, 0);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 100);
		break;
	case LED_COLOR_YELLOW:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 60);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	case LED_COLOR_PINK:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 60);
		break;
	default:
		board_led_pwm_set_duty(&board_led_battery_red, 0);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	}
}

void led_set_color_power(enum ec_led_color color)
{
	switch (color) {
	case LED_COLOR_WHITE:
		board_led_pwm_set_duty(&board_led_power_white, 100);
		break;
	default:
		board_led_pwm_set_duty(&board_led_power_white, 0);
		break;
	}
}
static void steelix_led_set_battery(void)
{
	static int battery_second;
	int soc = DIV_ROUND_NEAREST(charge_get_display_charge(), 10);
	uint32_t chgflags = charge_get_flags();

	battery_second++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
	case PWR_STATE_CHARGE_NEAR_FULL:
		if (chipset_in_state(CHIPSET_STATE_ON |
					CHIPSET_STATE_ANY_SUSPEND |
					CHIPSET_STATE_ANY_OFF)) {
			if (soc < 5) {
				led_set_color_battery(LED_COLOR_RED);
			} else if (soc >= 5 && soc < 30) {
				led_set_color_battery(LED_COLOR_PINK);
			} else if (soc >= 30 && soc < 60) {
				led_set_color_battery(LED_COLOR_BLUE);
			} else if (soc >= 60 && soc < 97) {
				led_set_color_battery(LED_COLOR_YELLOW);
			} else if (soc >= 97) {
				led_set_color_battery(LED_COLOR_GREEN);
			}
		}
		break;
	case PWR_STATE_DISCHARGE:
		led_set_color_battery(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		led_set_color_battery((battery_second & 0x1) ?
					LED_COLOR_RED : LED_OFF);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE. */
		if (chgflags & CHARGE_FLAG_FORCE_IDLE) {
			led_set_color_battery((battery_second & 0x2) ?
					LED_COLOR_RED : LED_COLOR_GREEN);
		} else {
			led_set_color_battery(LED_COLOR_RED);
		}
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void steelix_led_set_power(void)
{
	static int power_second;

	power_second++;

	switch (power_get_state()) {
	case PWR_STATE_CHARGE:
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_DISCHARGE:
		if (chipset_in_state(CHIPSET_STATE_ON)) {
			led_set_color_power(LED_COLOR_WHITE);
		} else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
			led_set_color_power((power_second & 0x2) ?
						LED_COLOR_WHITE : LED_OFF);
		} else if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
			led_set_color_power(LED_OFF);
		}
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_RED] = 1;
		brightness_range[EC_LED_COLOR_BLUE] = 1;
		brightness_range[EC_LED_COLOR_GREEN] = 1;
	} else if (led_id == EC_LED_ID_POWER_LED) {
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0) {
			led_set_color_battery(LED_COLOR_RED);
		} else if (brightness[EC_LED_COLOR_GREEN] != 0) {
			led_set_color_battery(LED_COLOR_GREEN);
		} else if (brightness[EC_LED_COLOR_BLUE] != 0) {
			led_set_color_battery(LED_COLOR_BLUE);
		} else {
			led_set_color_battery(LED_OFF);
		}
	} else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0) {
			led_set_color_power(LED_COLOR_WHITE);
		} else {
			led_set_color_power(LED_OFF);
		}
	}

	return EC_SUCCESS;
}

static void led_second(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		steelix_led_set_battery();
	}
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) {
		steelix_led_set_power();
	}
}
DECLARE_HOOK(HOOK_SECOND, led_second, HOOK_PRIO_DEFAULT);

void led_control(enum ec_led_id led_id, enum ec_led_state state)
{
	uint8_t br[EC_LED_COLOR_COUNT] = { 0 };

	if ((led_id != EC_LED_ID_RECOVERY_HW_REINIT_LED) &&
	    (led_id != EC_LED_ID_SYSRQ_DEBUG_LED)) {
		return;
	}

	if (state == LED_STATE_RESET) {
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		led_auto_control(EC_LED_ID_POWER_LED, 1);
		return;
	}

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);
	led_auto_control(EC_LED_ID_POWER_LED, 0);
	led_set_brightness(EC_LED_ID_BATTERY_LED, br);
	led_set_brightness(EC_LED_ID_POWER_LED, br);
}

static int command_pwmled(int argc, char **argv)
{
	char *e;
	int index;
	int duty;

	if (argc == 3) {
		index = strtoi(argv[1], &e, 0);
		duty = strtoi(argv[2], &e, 0);
		if (*e || index < -1 || index > 3 || duty < 0 || duty > 100)
			return EC_ERROR_PARAM1;
		led_auto_control(EC_LED_ID_BATTERY_LED, 0);
	} else {
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		return EC_ERROR_PARAM1;
	}
	switch (index) {
	case 1: /* Red LED */
		board_led_pwm_set_duty(&board_led_battery_red, duty);
		break;
	case 2: /* green LED */
		board_led_pwm_set_duty(&board_led_battery_green, duty);
		break;
	case 3: /* green LED */
		board_led_pwm_set_duty(&board_led_battery_blue, duty);
		break;
	default:
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		break;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pwmled, command_pwmled,
			"index duty",
			"Set the led pwm duty");
