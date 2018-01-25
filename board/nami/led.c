/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LED control for Nami
 */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "extpower.h"
#include "chipset.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

const enum ec_led_id supported_led_ids[] = {EC_LED_ID_LEFT_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

#define LED_TOTAL_2SECS_TICKS		2
#define LED_ON_1SEC_TICKS		1
#define LED_PULSE_US			(2 * SECOND)
/* 40 msec for nice and smooth transition. */
#define LED_PULSE_TICK_US		(40 * MSEC)

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_AMBER,
	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

enum led_states {
	STATE_CHARGING = 0,
	STATE_CHARGING_FULL,
	STATE_DISCHARGE_S0,
	STATE_DISCHARGE_S3,
	STATE_DISCHARGE_S5,
	STATE_BATTERY_ERROR,
	LED_NUM_STATES
};

static int set_color_Left(enum led_color color, int duty)
{
	if (duty < 0 || 100 < duty)
		return EC_ERROR_UNKNOWN;

	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_LED_WHITE, 0);
		pwm_set_duty(PWM_CH_LED_AMBER, 0);
		break;
	case LED_WHITE:
		pwm_set_duty(PWM_CH_LED_WHITE, duty);
		pwm_set_duty(PWM_CH_LED_AMBER, 0);
		break;
	case LED_AMBER:
		pwm_set_duty(PWM_CH_LED_WHITE, 0);
		pwm_set_duty(PWM_CH_LED_AMBER, duty);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int set_color(enum ec_led_id id, enum led_color color, int duty)
{
	switch (id) {
	case EC_LED_ID_LEFT_LED:
		return set_color_Left(color, duty);
	default:
		return EC_ERROR_UNKNOWN;
	}
}

/* When pulsing is enabled, brightness is incremented by <duty_inc> every
 * <interval> usec from 0 to 100% in LED_PULSE_US usec. Then it's decremented
 * likewise in LED_PULSE_US usec. */
static struct {
	uint32_t interval;
	int duty_inc;
	enum led_color color;
	int duty;
} led_pulse;

#define CONFIGURE_PULSE(interval, color) \
	configure_pulse((interval), 100 / (LED_PULSE_US / (interval)), (color))

static void configure_pulse(uint32_t interval, int duty_inc,
	enum led_color color)
{
	led_pulse.interval = interval;
	led_pulse.duty_inc = duty_inc;
	led_pulse.color = color;
	led_pulse.duty = 0;
}

static void pulse_led(enum led_color color)
{
	set_color(EC_LED_ID_LEFT_LED, color, led_pulse.duty);
	if (led_pulse.duty + led_pulse.duty_inc > 100)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	else if (led_pulse.duty + led_pulse.duty_inc < 0)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	led_pulse.duty += led_pulse.duty_inc;
}

static void led_pulse_tick(void);
DECLARE_DEFERRED(led_pulse_tick);
static void led_pulse_tick(void)
{
	uint32_t elapsed;
	uint32_t next = 0;
	uint32_t start = get_time().le.lo;
	static uint8_t pwm_enabled = 0;

	if (!pwm_enabled) {
		pwm_enable(PWM_CH_LED_WHITE, 1);
		pwm_enable(PWM_CH_LED_AMBER, 1);
		pwm_enabled = 1;
	}
	if (led_auto_control_is_enabled(EC_LED_ID_LEFT_LED))
		pulse_led(led_pulse.color);
	elapsed = get_time().le.lo - start;
	next = led_pulse.interval > elapsed ? led_pulse.interval - elapsed : 0;
	hook_call_deferred(&led_pulse_tick_data, next);
}

static void led_battery_error(void)
{
	static int error_ticks;

	set_color(EC_LED_ID_LEFT_LED,
		(error_ticks % LED_TOTAL_2SECS_TICKS
		<  LED_ON_1SEC_TICKS) ? LED_AMBER : LED_OFF, 100);
	error_ticks++;
}

static uint32_t led_interval;
static void led_blink_tick(void);
DECLARE_DEFERRED(led_blink_tick);
static void led_blink_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_LEFT_LED))
		led_battery_error();
	hook_call_deferred(&led_blink_tick_data, led_interval);
}

static void led_alert(uint32_t interval)
{
	if (interval)
		led_interval = interval;
	else
		led_interval = -1;
	led_blink_tick();
}

static void led_update(enum led_states desired_state)
{
	switch (desired_state) {
	case STATE_CHARGING:
		hook_call_deferred(&led_pulse_tick_data, -1);
		hook_call_deferred(&led_blink_tick_data, -1);
		set_color(EC_LED_ID_LEFT_LED, LED_AMBER, 100);
		break;
	case STATE_CHARGING_FULL:
	case STATE_DISCHARGE_S0:
		hook_call_deferred(&led_pulse_tick_data, -1);
		hook_call_deferred(&led_blink_tick_data, -1);
		set_color(EC_LED_ID_LEFT_LED, LED_WHITE, 100);
		break;
	case STATE_DISCHARGE_S3:
		/* Pulsing (rising for 2 sec , falling for 2 sec) */
		hook_call_deferred(&led_blink_tick_data, -1);
		CONFIGURE_PULSE(LED_PULSE_TICK_US, LED_AMBER);
		led_pulse_tick();
		break;
	case STATE_DISCHARGE_S5:
		hook_call_deferred(&led_pulse_tick_data, -1);
		hook_call_deferred(&led_blink_tick_data, -1);
		set_color(EC_LED_ID_LEFT_LED, LED_OFF, 0);
		break;
	case STATE_BATTERY_ERROR:
		/* Amber on 1sec off 1sec */
		hook_call_deferred(&led_pulse_tick_data, -1);
		led_alert(1 * SECOND);
		break;
	default:
		hook_call_deferred(&led_pulse_tick_data, -1);
		hook_call_deferred(&led_blink_tick_data, -1);
		break;
	}
}

static void led_ac_mode(void)
{
	if ((charge_get_state() == PWR_STATE_CHARGE_NEAR_FULL) ||
		(charge_get_state() == PWR_STATE_IDLE))
		led_update(STATE_CHARGING_FULL);
	else
		led_update(STATE_CHARGING);
}

static void led_battery(void)
{

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		led_update(STATE_CHARGING);
		break;
	case PWR_STATE_DISCHARGE:
		if (chipset_in_state(CHIPSET_STATE_ON))
			led_update(STATE_DISCHARGE_S0);
		else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			led_update(STATE_DISCHARGE_S3);
		else
			led_update(STATE_DISCHARGE_S5);
		break;
	case PWR_STATE_ERROR:
		led_update(STATE_BATTERY_ERROR);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		led_update(STATE_CHARGING_FULL);
		break;
	default:
		/* Other states don't alter LED behavior */
		hook_call_deferred(&led_pulse_tick_data, -1);
		hook_call_deferred(&led_blink_tick_data, -1);
		break;
	}
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, led_battery, HOOK_PRIO_DEFAULT);

static void led_ac_change(void);
static void led_ac_change(void)
{
	if (extpower_is_present())
		led_ac_mode();
	else {
		if (chipset_in_state(CHIPSET_STATE_ON))
			led_update(STATE_DISCHARGE_S0);
		else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			led_update(STATE_DISCHARGE_S3);
		else
			led_update(STATE_DISCHARGE_S5);
	}
}
DECLARE_DEFERRED(led_ac_change);

static void led_ac_change_deferred(void)
{
	hook_call_deferred(&led_ac_change_data, 500 * MSEC);
}
DECLARE_HOOK(HOOK_AC_CHANGE, led_ac_change_deferred, HOOK_PRIO_DEFAULT);

static void led_suspend(void)
{
	if (extpower_is_present())
		led_ac_mode();
	else
		led_update(STATE_DISCHARGE_S3);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, led_suspend, HOOK_PRIO_DEFAULT);

static void led_shutdown(void)
{
	if (extpower_is_present())
		led_ac_mode();
	else
		led_update(STATE_DISCHARGE_S5);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, led_shutdown, HOOK_PRIO_DEFAULT);

static void led_resume(void)
{
	if (extpower_is_present())
		led_ac_mode();
	else
		led_update(STATE_DISCHARGE_S0);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, led_resume, HOOK_PRIO_DEFAULT);

static int command_led(int argc, char **argv)
{
	enum ec_led_id id = EC_LED_ID_LEFT_LED;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "debug")) {
		led_auto_control(id, !led_auto_control_is_enabled(id));
		ccprintf("o%s\n", led_auto_control_is_enabled(id) ? "ff" : "n");
	} else if (!strcasecmp(argv[1], "off")) {
		set_color(id, LED_OFF, 0);
	} else if (!strcasecmp(argv[1], "white")) {
		set_color(id, LED_WHITE, 100);
	} else if (!strcasecmp(argv[1], "amber")) {
		set_color(id, LED_AMBER, 100);
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"[debug|white|amber|off]",
			"Turn on/off LED.");

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_WHITE] = 100;
	brightness_range[EC_LED_COLOR_AMBER] = 100;
}

int led_set_brightness(enum ec_led_id id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_WHITE])
		return set_color(id, LED_WHITE, brightness[EC_LED_COLOR_WHITE]);
	else if (brightness[EC_LED_COLOR_AMBER])
		return set_color(id, LED_AMBER, brightness[EC_LED_COLOR_AMBER]);
	else
		return set_color(id, LED_OFF, 0);
}
