/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Steelix
 */

#include <init.h>
#include <ap_power/ap_power.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include "board_led.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "ec_commands.h"
#include "hooks.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "power.h"
#include "util.h"

#define LED_BAT_S3_OFF_TIME_MS 2000
#define LED_BAT_S3_PWM_RESCALE 5
#define LED_BAT_S3_TICK_MS 50

#define TICKS_STEP1_BRIGHTER 0
#define TICKS_STEP2_DIMMER 20
#define TICKS_STEP3_OFF 40

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

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

static int ticks;

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

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

void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_RED:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	case EC_LED_COLOR_WHITE:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 40);
		board_led_pwm_set_duty(&board_led_battery_blue, 30);
		break;
	case EC_LED_COLOR_AMBER:
		board_led_pwm_set_duty(&board_led_battery_red, 100);
		board_led_pwm_set_duty(&board_led_battery_green, 15);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	default:
		board_led_pwm_set_duty(&board_led_battery_red, 0);
		board_led_pwm_set_duty(&board_led_battery_green, 0);
		board_led_pwm_set_duty(&board_led_battery_blue, 0);
		break;
	}
}

void led_set_color_power(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_WHITE:
		board_led_pwm_set_duty(&board_led_power_white, 100);
		break;
	default:
		board_led_pwm_set_duty(&board_led_power_white, 0);
		break;
	}
}
static void steelix_led_set_battery(void)
{
	static int battery_ticks;
	int soc = DIV_ROUND_NEAREST(charge_get_display_charge(), 10);
	uint32_t chgflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
	case PWR_STATE_CHARGE_NEAR_FULL:
		if (chipset_in_state(CHIPSET_STATE_ON |
					CHIPSET_STATE_ANY_SUSPEND |
					CHIPSET_STATE_ANY_OFF)) {
			if (soc <= 90) {
				led_set_color_battery(EC_LED_COLOR_AMBER);
			} else {
				led_set_color_battery(EC_LED_COLOR_WHITE);
			}
		}
		break;
	case PWR_STATE_DISCHARGE:
		led_set_color_battery(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		led_set_color_battery((battery_ticks % 10 < 5) ?
					EC_LED_COLOR_RED : LED_OFF);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE. */
		if (chgflags & CHARGE_FLAG_FORCE_IDLE) {
			led_set_color_battery((battery_ticks % 20 < 10) ?
					EC_LED_COLOR_RED : EC_LED_COLOR_GREEN);
		} else {
			led_set_color_battery(EC_LED_COLOR_RED);
		}
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void steelix_led_set_power(void)
{
	switch (power_get_state()) {
	case PWR_STATE_CHARGE:
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_DISCHARGE:
		if (chipset_in_state(CHIPSET_STATE_ON)) {
			led_set_color_power(EC_LED_COLOR_WHITE);
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
		brightness_range[EC_LED_COLOR_WHITE] = 1;
		brightness_range[EC_LED_COLOR_AMBER] = 1;
	} else if (led_id == EC_LED_ID_POWER_LED) {
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0) {
			led_set_color_battery(EC_LED_COLOR_RED);
		} else if (brightness[EC_LED_COLOR_WHITE] != 0) {
			led_set_color_battery(EC_LED_COLOR_WHITE);
		} else if (brightness[EC_LED_COLOR_AMBER] != 0) {
			led_set_color_battery(EC_LED_COLOR_AMBER);
		} else {
			led_set_color_battery(LED_OFF);
		}
	} else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_WHITE] != 0) {
			led_set_color_power(EC_LED_COLOR_WHITE);
		} else {
			led_set_color_power(LED_OFF);
		}
	}

	return EC_SUCCESS;
}

/* Called by hook task every 200 ms */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		steelix_led_set_battery();
	}
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) {
		steelix_led_set_power();
	}
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

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

static void suspend_led_update_deferred(void);
DECLARE_DEFERRED(suspend_led_update_deferred);

static void suspend_led_update_deferred(void)
{
	int delay = LED_BAT_S3_TICK_MS * MSEC;

	ticks++;

	/* 1s gradual on, 1s gradual off, 2s off */
	if (ticks <= TICKS_STEP2_DIMMER) {
		board_led_pwm_set_duty(&board_led_power_white,
					ticks * LED_BAT_S3_PWM_RESCALE);
	} else if (ticks <= TICKS_STEP3_OFF) {
		board_led_pwm_set_duty(&board_led_power_white,
			(TICKS_STEP3_OFF - ticks) * LED_BAT_S3_PWM_RESCALE);
	} else {
		ticks = TICKS_STEP1_BRIGHTER;
		delay = LED_BAT_S3_OFF_TIME_MS * MSEC;
	}

	hook_call_deferred(&suspend_led_update_deferred_data, delay);
}

static void suspend_led_handler(struct ap_power_ev_callback *cb,
				    struct ap_power_ev_data data)
{
	int value;

	switch (data.event) {
	default:
		return;

	case AP_POWER_RESUME:
		/* Called on AP S3 -> S0 transition */
		value = -1;
		break;

	case AP_POWER_SUSPEND:
		/* Called on AP S0 -> S3 transition */
		value = 0;
		break;

	case AP_POWER_SHUTDOWN:
		/* Called on AP S3 -> S5 transition */
		value = -1;
		break;
	}
	hook_call_deferred(&suspend_led_update_deferred_data, value);
}

static int install_suspend_led_handler(const struct device *unused)
{
	static struct ap_power_ev_callback cb;

	/*
	 * Add a callback for suspend/resume to
	 * control the keyboard backlight.
	 */
	ap_power_ev_init_callback(&cb, suspend_led_handler,
				  AP_POWER_RESUME | AP_POWER_SUSPEND |
				  AP_POWER_SHUTDOWN);
	ap_power_ev_add_callback(&cb);
	return 0;
}

SYS_INIT(install_suspend_led_handler, APPLICATION, 1);
