/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Power LED control for Shotzo.
 * Solid white - active power
 * Breathing white, 1s to 100% and 1s to 0% - suspend
 * Blinking quicky white, 0.5s on and 0.5s off - alert
 * 2 long 2 short white, long for 1s, short for 0.5s and interval
 * is 0.5s - critical
 * Off - shut down
 */

#include "chipset.h"
#include "console.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

/*
 * Due to the CSME-Lite processing, upon startup the CPU transitions through
 * S0->S3->S5->S3->S0, causing the LED to turn on/off/on, so
 * delay turning off the LED during suspend/shutdown.
 */
#define LED_CPU_DELAY_MS	(2000 * MSEC)

/* When pulsing is enabled, brightness is incremented from 0 to 100%
 * in LED_PULSE_US usec. Then it's decremented likewise.
 */
#define LED_PULSE_US		(1 * SECOND)
/* 40 msec for nice and smooth transition. */
#define LED_PULSE_TICK_US	(40 * MSEC)

const enum ec_led_id supported_led_ids[] = {EC_LED_ID_POWER_LED};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_RED,
	LED_WHITE,
	LED_AMBER,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static struct {
	uint32_t interval;
	int duty_inc;
	enum led_color color;
	int duty;
} led_pulse;

/* When pulsing is enabled, brightness is incremented by <duty_inc> every
 * <interval> usec from 0 to 100% Then it's decremented likewise.
 */
static void config_tick(uint32_t interval, int duty_inc, enum led_color color)
{
	led_pulse.interval = interval;
	led_pulse.duty_inc = duty_inc;
	led_pulse.color = color;
	led_pulse.duty = 0;
}

#define CONFIGURE_TICK(interval, color) \
	config_tick((interval), 100 / (LED_PULSE_US / (interval)), (color))

static int led_set_color_power(enum led_color color, int duty)
{
	int white = 0;
	int red = 0;

	if (duty < 0 || 100 < duty)
		return EC_ERROR_UNKNOWN;

	switch (color) {
	case LED_OFF:
		break;
	case LED_RED:
		red = 1;
		break;
	case LED_WHITE:
		white = 1;
		break;
	case LED_AMBER:
		white = 1;
		red = 1;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	if (red)
		pwm_set_duty(PWM_CH_LED_RED, duty);
	else
		pwm_set_duty(PWM_CH_LED_RED, 0);

	if (white)
		pwm_set_duty(PWM_CH_LED_WHITE, duty);
	else
		pwm_set_duty(PWM_CH_LED_WHITE, 0);

	return EC_SUCCESS;
}

static int led_set_color(enum ec_led_id led_id, enum led_color color, int duty)
{
	int rv;

	switch (led_id) {
	case EC_LED_ID_POWER_LED:
		rv = led_set_color_power(color, duty);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return rv;
}

static void pulse_power_led(enum led_color color)
{
	led_set_color(EC_LED_ID_POWER_LED, color, led_pulse.duty);
	if (led_pulse.duty + led_pulse.duty_inc > 100)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	else if (led_pulse.duty + led_pulse.duty_inc < 0)
		led_pulse.duty_inc = led_pulse.duty_inc * -1;
	led_pulse.duty += led_pulse.duty_inc;
	led_pulse.duty = MIN(100, MAX(led_pulse.duty, 0));
}

static void led_tick(void);
DECLARE_DEFERRED(led_tick);
static void led_tick(void)
{
	uint32_t elapsed;
	uint32_t next = 0;
	uint32_t start = get_time().le.lo;

	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		pulse_power_led(led_pulse.color);
	elapsed = get_time().le.lo - start;
	next = led_pulse.interval > elapsed ? led_pulse.interval - elapsed : 0;
	hook_call_deferred(&led_tick_data, next);
}

/* When blinking is enabled, led will blinking according to led_blinking_array,
 * 1 means led on, 0 means led off, -1 means end and go to head of array.
 * The interval is LED_BLINKING_MS.
 */
#define LED_BLINKING_MS		(500 * MSEC)
#define LED_BLINKING_ARRAY_MAX	15
static int led_blinking_array[LED_BLINKING_ARRAY_MAX];
static int led_blinking_index;
static void led_blinking(void);
DECLARE_DEFERRED(led_blinking);
static void led_blinking(void)
{
	uint32_t elapsed;
	uint32_t next = 0;
	uint32_t start = get_time().le.lo;

	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) {
		switch (led_blinking_array[led_blinking_index]) {
		case 0:
			led_set_color(EC_LED_ID_POWER_LED, LED_OFF, 0);
			break;
		case 1:
			led_set_color(EC_LED_ID_POWER_LED, LED_WHITE, 100);
			break;
		default:
			led_blinking_index = 0;
		}

		led_blinking_index += 1;
		if (led_blinking_array[led_blinking_index] == -1)
			led_blinking_index = 0;
	}

	elapsed = get_time().le.lo - start;
	next = elapsed < LED_BLINKING_MS ? LED_BLINKING_MS - elapsed : 0;
	hook_call_deferred(&led_blinking_data, next);
}

static void led_suspend(void)
{
	CONFIGURE_TICK(LED_PULSE_TICK_US, LED_WHITE);
	led_tick();
}
DECLARE_DEFERRED(led_suspend);

static void led_shutdown(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_color(EC_LED_ID_POWER_LED, LED_OFF, 0);
}
DECLARE_DEFERRED(led_shutdown);

static void led_suspend_hook(void)
{
	hook_call_deferred(&led_tick_data, -1);
	hook_call_deferred(&led_shutdown_data, -1);
	hook_call_deferred(&led_suspend_data, LED_CPU_DELAY_MS);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, led_suspend_hook, HOOK_PRIO_DEFAULT);

static void led_shutdown_hook(void)
{
	hook_call_deferred(&led_tick_data, -1);
	hook_call_deferred(&led_suspend_data, -1);
	hook_call_deferred(&led_shutdown_data, LED_CPU_DELAY_MS);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, led_shutdown_hook, HOOK_PRIO_DEFAULT);

static void led_resume_hook(void)
{
	/* Assume there is no race condition with led_pulse, which also
	 * runs in hook_task.
	 */
	hook_call_deferred(&led_tick_data, -1);
	/*
	 * Avoid invoking the suspend/shutdown delayed hooks.
	 */
	hook_call_deferred(&led_suspend_data, -1);
	hook_call_deferred(&led_shutdown_data, -1);

	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_color(EC_LED_ID_POWER_LED, LED_WHITE, 100);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, led_resume_hook, HOOK_PRIO_DEFAULT);

const int led_alert_array[] = {1, 0, -1};
const int led_alert_count = ARRAY_SIZE(led_alert_array);
void led_alert(int enable)
{
	if (enable) {
		/* Overwrite the current signal */
		hook_call_deferred(&led_tick_data, -1);
		memcpy(led_blinking_array, led_alert_array, led_alert_count);
		led_blinking_index = 0;
		led_blinking();
	} else {
		/* Restore the previous signal */
		if (chipset_in_state(CHIPSET_STATE_ON))
			led_resume_hook();
		else if (chipset_in_state(CHIPSET_STATE_SUSPEND))
			led_suspend_hook();
		else if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			led_shutdown_hook();
	}
}

const int led_critical_array[] = {1, 1, 0, 1, 1, 0, 1, 0, 1, 0, -1};
const int led_critical_count = ARRAY_SIZE(led_critical_array);
__override void show_critical_error(void)
{
	hook_call_deferred(&led_tick_data, -1);
	memcpy(led_blinking_array, led_critical_array, led_critical_count);
	led_blinking_index = 0;
	led_blinking();
}

static void led_init(void)
{
	pwm_enable(PWM_CH_LED_WHITE, 1);
	pwm_enable(PWM_CH_LED_RED, 1);
}
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_INIT_PWM + 1);

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_RED] = 100;
		brightness_range[EC_LED_COLOR_WHITE] = 100;
		brightness_range[EC_LED_COLOR_AMBER] = 100;
		break;
	default:
		break;
	}
}

int led_set_brightness(enum ec_led_id id, const uint8_t *brightness)
{
	if (brightness[EC_LED_COLOR_RED] != 0)
		led_set_color(id, LED_RED, brightness[EC_LED_COLOR_RED]);
	if (brightness[EC_LED_COLOR_WHITE] != 0)
		led_set_color(id, LED_WHITE, brightness[EC_LED_COLOR_WHITE]);
	if (brightness[EC_LED_COLOR_AMBER] != 0)
		led_set_color(id, LED_AMBER, brightness[EC_LED_COLOR_AMBER]);
	else
		led_set_color(id, LED_OFF, 0);

	return EC_SUCCESS;
}

static int command_led(int argc, char **argv)
{
	enum ec_led_id id = EC_LED_ID_POWER_LED;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "debug")) {
		led_auto_control(id, !led_auto_control_is_enabled(id));
		ccprintf("o%s\n", led_auto_control_is_enabled(id) ? "ff" : "n");
	} else if (!strcasecmp(argv[1], "off")) {
		led_set_color(id, LED_OFF, 0);
	} else if (!strcasecmp(argv[1], "red")) {
		led_set_color(id, LED_RED, 100);
	} else if (!strcasecmp(argv[1], "white")) {
		led_set_color(id, LED_WHITE, 100);
	} else if (!strcasecmp(argv[1], "amber")) {
		led_set_color(id, LED_AMBER, 100);
	} else if (!strcasecmp(argv[1], "alert")) {
		led_alert(1);
	} else if (!strcasecmp(argv[1], "crit")) {
		show_critical_error();
	} else {
		return EC_ERROR_PARAM1;
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(led, command_led,
			"[debug|red|white|amber|off|alert|crit]",
			"Turn on/off LED.");
