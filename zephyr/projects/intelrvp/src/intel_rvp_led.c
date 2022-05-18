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

#define FAST_PULSE_PERIOD  (250 / 125)		/* 250 ms */
#define SLOW_PULSE_PERIOD  ((2 * MSEC) / 125)	/* 2 sec  */

struct _pulse_data {
	uint8_t led_is_pulsing;
	uint8_t led_pulse_period;
	enum ec_led_colors led_pulse_color;
};

static struct _pulse_data pulse[CONFIG_LED_PWM_COUNT];

static void pulse_led_deferred(void);
DECLARE_DEFERRED(pulse_led_deferred);

static void init_rvp_leds_off(void)
{
	/* Turn off LEDs such that they are in a known state with zero duty. */
	set_pwm_led_color(PWM_LED0, -1);
	set_pwm_led_color(PWM_LED1, -1);
}
DECLARE_HOOK(HOOK_INIT, init_rvp_leds_off, HOOK_PRIO_POST_PWM);

static void pulse_led_deferred(void)
{
	static uint8_t tick_count[CONFIG_LED_PWM_COUNT];
	int led_index = 0;
	bool call_deferred = 0;

	for (led_index = 0; led_index < CONFIG_LED_PWM_COUNT; led_index++) {
		if (!pulse[led_index].led_is_pulsing) {
			tick_count[led_index] = 0;
			continue;
		}

		if (tick_count[led_index] < (
					pulse[led_index].led_pulse_period / 2))
			set_pwm_led_color(led_index,
					pulse[led_index].led_pulse_color);
		else
			set_pwm_led_color(led_index, -1);

		tick_count[led_index] = (tick_count[led_index] + 1) %
					pulse[led_index].led_pulse_period;
		call_deferred = 1;
	}

	if (call_deferred)
		hook_call_deferred(&pulse_led_deferred_data, PULSE_TICK);
}

static void pulse_leds(enum pwm_led_id led_id, enum ec_led_colors color,
								int period)
{
	pulse[led_id].led_pulse_color  = color;
	pulse[led_id].led_pulse_period = period;
	pulse[led_id].led_is_pulsing   = 1;

	pulse_led_deferred();
}

static void update_charger_led(void)
{
	enum charge_state chg_st = charge_get_state();

	/*
	 * The colors listed below are the default, but can be overridden.
	 *
	 * Fast Flash = Charging error
	 * Slow Flash = Discharging
	 * LED on     = Charging
	 * LED off    = No Charger connected
	 */
	if (chg_st == PWR_STATE_CHARGE ||
	    chg_st == PWR_STATE_CHARGE_NEAR_FULL) {
		pulse[PWM_LED1].led_is_pulsing = 0;
		set_pwm_led_color(PWM_LED1, EC_LED_COLOR_GREEN);
	} else if (chg_st == PWR_STATE_DISCHARGE ||
		  chg_st == PWR_STATE_DISCHARGE_FULL) {
		if (extpower_is_present()) {
			pulse[PWM_LED1].led_is_pulsing = 0;
			set_pwm_led_color(PWM_LED1, EC_LED_COLOR_GREEN);
		} else {
			/* Discharging or not charging. */
			pulse_leds(PWM_LED1, EC_LED_COLOR_GREEN,
							SLOW_PULSE_PERIOD);
		}
	} else if (chg_st == PWR_STATE_ERROR) {
		/* 250 ms period, 100% duty cycle. */
		pulse_leds(PWM_LED1, EC_LED_COLOR_GREEN,
							FAST_PULSE_PERIOD);
	} else {
		pulse[PWM_LED1].led_is_pulsing = 0;
		set_pwm_led_color(PWM_LED1, -1);
	}
}

static void update_battery_led(void)
{
	/*
	 * Fast Flash = Low Battery
	 * Slow Flash = Normal Battery
	 * LED on     = Full Battery
	 * LED off    = No Battery
	 */
	if (battery_is_present() == BP_YES) {
		int batt_percentage = charge_get_percent();

		if (batt_percentage < LOW_BATTERY_PERCENTAGE) {
			/* Flash faster (250 ms period, 100% duty cycle) */
			pulse_leds(PWM_LED0, EC_LED_COLOR_GREEN,
							FAST_PULSE_PERIOD);
		} else if (batt_percentage < NORMAL_BATTERY_PERCENTAGE) {
			/* Flash slower (2 second period, 100% duty cycle) */
			pulse_leds(PWM_LED0, EC_LED_COLOR_GREEN,
							SLOW_PULSE_PERIOD);
		} else {
			/* Full Battery */
			pulse[PWM_LED0].led_is_pulsing = 0;
			set_pwm_led_color(PWM_LED0, EC_LED_COLOR_GREEN);
		}
	} else {
		pulse[PWM_LED0].led_is_pulsing = 0;
		set_pwm_led_color(PWM_LED0, -1);
	}
}

static void update_led(void)
{
	update_battery_led();
	update_charger_led();
}
DECLARE_HOOK(HOOK_SECOND, update_led, HOOK_PRIO_DEFAULT);
