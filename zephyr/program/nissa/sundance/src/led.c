/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sundance specific PWM LED settings: there is one LED on the motherboard.
 * LED colors are white or amber. The default behavior is related to the
 * charging process. The amber light with AC adapter is on when the system is
 * in s0/s0ix/s5 state. When battery error, the LED will light up for 1 second
 * and then turn off for 2 seconds. When the system is in the s0 state and the
 * battery is fully charged, the whilt LED will lighten. In the case of battery
 * only, when the system is in the s0 state and battery is fully charged, the
 * white LED will light up. In other case, the LED status is off.
 */


#include "board_led.h"
#include "chipset.h"
#include "common.h"
#include "ec_commands.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "util.h"

#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_led, LOG_LEVEL_ERR);

#define BOARD_LED_PWM_PERIOD_NS BOARD_LED_HZ_TO_PERIOD_NS(100)

static const struct board_led_pwm_dt_channel board_led_battery_red =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_battery_red));
static const struct board_led_pwm_dt_channel board_led_battery_green =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_battery_green));
static const struct board_led_pwm_dt_channel board_led_power_white =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(led_power_white));

__override const int led_charge_lvl_1 = 0;
__override const int led_charge_lvl_2 = 100;

__override struct led_descriptor
	led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
		[STATE_CHARGING_LVL_1] = { { EC_LED_COLOR_AMBER,
					     LED_INDEFINITE } },
		[STATE_CHARGING_LVL_2] = { { EC_LED_COLOR_AMBER,
					     LED_INDEFINITE } },
		[STATE_CHARGING_FULL_CHARGE] = { { EC_LED_COLOR_WHITE,
						   LED_INDEFINITE } },
		[STATE_CHARGING_FULL_S5] = { { LED_OFF, LED_INDEFINITE } },
		[STATE_DISCHARGE_S0] = { { EC_LED_COLOR_WHITE,
					   LED_INDEFINITE } },
		[STATE_DISCHARGE_S0_BAT_LOW] = { { EC_LED_COLOR_WHITE,
						   LED_INDEFINITE } },
		[STATE_DISCHARGE_S3] = { { LED_OFF, LED_INDEFINITE } },
		[STATE_DISCHARGE_S5] = { { LED_OFF, LED_INDEFINITE } },
		[STATE_BATTERY_ERROR] = { { EC_LED_COLOR_AMBER,
					    1 * LED_ONE_SEC },
					  { LED_OFF, 2 * LED_ONE_SEC } },
		[STATE_FACTORY_TEST] = { { EC_LED_COLOR_AMBER,
					   2 * LED_ONE_SEC },
					 { EC_LED_COLOR_WHITE,
					   2 * LED_ONE_SEC } },
	};

const enum ec_led_id supported_led_ids[] = { EC_LED_ID_BATTERY_LED };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

static void board_led_pwm_set_duty(const struct board_led_pwm_dt_channel *ch,
				   int percent)
{
	uint32_t pulse_ns;
	int rv;

	if (!device_is_ready(ch->dev)) {
		LOG_ERR("device %s not ready", ch->dev->name);
		return;
	}

	pulse_ns = DIV_ROUND_NEAREST(BOARD_LED_PWM_PERIOD_NS * percent, 100);

	LOG_DBG("Board LED PWM %s set percent (%d), pulse %d", ch->dev->name,
		percent, pulse_ns);

	rv = pwm_set(ch->dev, ch->channel, BOARD_LED_PWM_PERIOD_NS, pulse_ns,
		     ch->flags);
	if (rv) {
		LOG_ERR("pwm_set() failed %s (%d)", ch->dev->name, rv);
	}
}

__override void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_AMBER:
		board_led_pwm_set_duty(&board_led_battery_red, 30);
		board_led_pwm_set_duty(&board_led_battery_green, 100);
		break;
	case EC_LED_COLOR_WHITE:
		board_led_pwm_set_duty(&board_led_power_white, 100);
		break;
	default: /* LED_OFF and other unsupported colors */
		board_led_pwm_set_duty(&board_led_battery_red, 0);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_power_white, 0);
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_RED] = 1;
		brightness_range[EC_LED_COLOR_GREEN] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0) {
			led_set_color_battery(EC_LED_COLOR_RED);
		} else if (brightness[EC_LED_COLOR_AMBER] != 0) {
			led_set_color_battery(EC_LED_COLOR_AMBER);
		} else if (brightness[EC_LED_COLOR_WHITE] != 0) {
			led_set_color_battery(EC_LED_COLOR_WHITE);
		} else if (brightness[EC_LED_COLOR_GREEN] != 0) {
			led_set_color_battery(EC_LED_COLOR_GREEN);
		} else {
			led_set_color_battery(LED_OFF);
		}
	}
	return EC_SUCCESS;
}

__override enum led_states board_led_get_state(enum led_states desired_state)
{
	if (desired_state == STATE_CHARGING_FULL_CHARGE) {
		if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			desired_state = STATE_CHARGING_FULL_S5;
	}
	return desired_state;
}

__override void led_control(enum ec_led_id led_id, enum ec_led_state state)
{
	if ((led_id != EC_LED_ID_RECOVERY_HW_REINIT_LED) &&
	    (led_id != EC_LED_ID_SYSRQ_DEBUG_LED))
		return;

	if (state == LED_STATE_RESET) {
		led_auto_control(EC_LED_ID_BATTERY_LED, 1);
		return;
	}

	led_auto_control(EC_LED_ID_BATTERY_LED, 0);

	led_set_color_battery(state ? EC_LED_COLOR_RED : EC_LED_COLOR_INVALID);
}
