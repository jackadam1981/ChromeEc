/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "led_common.h"
#include "led_pwm.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

/* Battery percentage thresholds to blink at different rates. */
#define LOW_BATTERY_PERCENTAGE 10
#define NORMAL_BATTERY_PERCENTAGE 90

#define PULSE_TICK (125 * MSEC)

static uint8_t led0_is_pulsing, led1_is_pulsing;
static uint8_t led0_pulse_period, led1_pulse_period;
static uint8_t led0_pulse_ontime, led1_pulse_ontime;
static enum ec_led_colors led0_pulse_color, led1_pulse_color;

static void update_battery_led(void);
static void update_charger_led(void);
static void pulse_led0_deferred(void);
static void pulse_led1_deferred(void);

DECLARE_DEFERRED(pulse_led0_deferred);
DECLARE_DEFERRED(pulse_led1_deferred);


static void init_rvp_leds_off(void)
{
	/* Turn off LEDs such that they are in a known state with zero duty. */
	set_pwm_led_color(PWM_LED0, -1);
	set_pwm_led_color(PWM_LED1, -1);
}
DECLARE_HOOK(HOOK_INIT, init_rvp_leds_off, HOOK_PRIO_POST_PWM);

static void pulse_led0_deferred(void)
{
	static uint8_t tick_count;

	if (!led0_is_pulsing) {
		tick_count = 0;
		/*
		 * Since we're not pulsing anymore, turn the colors off in case
		 * we were in the "on" time.
		 */
		set_pwm_led_color(PWM_LED0, -1);
		/* Then show the desired state. */
		update_battery_led();
		return;
	}

	if (tick_count < led0_pulse_ontime)
		set_pwm_led_color(PWM_LED0, led0_pulse_color);
	else
		set_pwm_led_color(PWM_LED0, -1);

	tick_count = (tick_count + 1) % led0_pulse_period;
	hook_call_deferred(&pulse_led0_deferred_data, PULSE_TICK);
}

static void pulse_led1_deferred(void)
{
	static uint8_t tick_count;

	if (!led1_is_pulsing) {
		tick_count = 0;
		/*
		 * Since we're not pulsing anymore, turn the colors off in case
		 * we were in the "on" time.
		 */
		set_pwm_led_color(PWM_LED1, -1);
		/* Then show the desired state. */
		update_charger_led();
		return;
	}

	if (tick_count < led1_pulse_ontime)
		set_pwm_led_color(PWM_LED1, led1_pulse_color);
	else
		set_pwm_led_color(PWM_LED1, -1);

	tick_count = (tick_count + 1) % led1_pulse_period;
	hook_call_deferred(&pulse_led1_deferred_data, PULSE_TICK);
}

static void pulse_leds(enum pwm_led_id led_id, enum ec_led_colors color,
							int ontime, int period)
{
	if (led_id == PWM_LED0) {
		led0_pulse_color = color;
		led0_pulse_ontime = ontime;
		led0_pulse_period = period;
		led0_is_pulsing = 1;
		pulse_led0_deferred();
	} else if (led_id == PWM_LED1) {
		led1_pulse_color = color;
		led1_pulse_ontime = ontime;
		led1_pulse_period = period;
		led1_is_pulsing = 1;
		pulse_led1_deferred();
	}
}

static void update_charger_led(void)
{
	enum charge_state chg_st = charge_get_state();

	/*
	 * The colors listed below are the default, but can be overridden.
	 *
	 * Fast Flash == Charging error
	 * Slow Flash == Discharging
	 * LED on     == Charging
	 * LED off    == No Charger connected
	 */
	if (chg_st == PWR_STATE_CHARGE ||
	    chg_st == PWR_STATE_CHARGE_NEAR_FULL) {
		led1_is_pulsing = 0;
		set_pwm_led_color(PWM_LED1, EC_LED_COLOR_GREEN);
	} else if (chg_st == PWR_STATE_DISCHARGE ||
		  chg_st == PWR_STATE_DISCHARGE_FULL) {
		if (extpower_is_present()) {
			led1_is_pulsing = 0;
			set_pwm_led_color(PWM_LED1, EC_LED_COLOR_GREEN);
		} else {
			/* Discharging or not charging. */
			pulse_leds(PWM_LED1, EC_LED_COLOR_GREEN, 8, 16);
		}
	} else if (chg_st == PWR_STATE_ERROR) {
		/* 250 ms period, 100% duty cycle. */
		pulse_leds(PWM_LED1, EC_LED_COLOR_GREEN, 1, 2);
	} else {
		led1_is_pulsing = 0;
		set_pwm_led_color(PWM_LED1, -1);
	}
}
DECLARE_HOOK(HOOK_TICK, update_charger_led, HOOK_PRIO_DEFAULT);

static void update_battery_led(void)
{
	int batt_percentage = charge_get_percent();

	/*
	 * Fast Flash == Low Battery
	 * Slow Flash == Normal Battery
	 * LED on     == Full Battery
	 * LED off    == No Battery
	 */
	if (battery_is_present() == BP_YES) {
		if (batt_percentage < LOW_BATTERY_PERCENTAGE) {
			/* Flash faster (250 ms period, 100% duty cycle) */
			pulse_leds(PWM_LED0, EC_LED_COLOR_GREEN, 1, 2);
		} else if (batt_percentage < NORMAL_BATTERY_PERCENTAGE) {
			/* Flash slower (2 second period, 100% duty cycle) */
			pulse_leds(PWM_LED0, EC_LED_COLOR_GREEN, 8, 16);
		} else {
			/* Full Battery */
			led0_is_pulsing = 0;
			set_pwm_led_color(PWM_LED0, EC_LED_COLOR_GREEN);
		}
	} else {
		led0_is_pulsing = 0;
		set_pwm_led_color(PWM_LED0, -1);
	}
}
DECLARE_HOOK(HOOK_TICK, update_battery_led, HOOK_PRIO_DEFAULT);
