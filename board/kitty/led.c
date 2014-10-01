/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power LED control for Kitty
 */
#include "clock.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "power_led.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#ifdef CONFIG_LED_CUSTOM

/* Define blink time */
#define LED_LIGHT_TIME		(1000 * MSEC)  /* LED light 1 second */
#define LED_DARK_TIME		(3000 * MSEC) /* LED dark 3 second */

static enum powerled_state led_state = POWERLED_STATE_ON;
static int power_led_percent = 100;

void powerled_set_state(enum powerled_state new_state)
{
	led_state = new_state;
	/* Wake up the task */
	task_wake(TASK_ID_POWERLED);
}

static void power_led_set_duty(int percent)
{
	ASSERT((percent >= 0) && (percent <= 100));
	power_led_percent = percent;
	pwm_set_duty(PWM_CH_POWER_LED, percent);
}

static void power_led_use_pwm(void)
{
	pwm_enable(PWM_CH_POWER_LED, 1);
	power_led_set_duty(100);
}

static int power_led_step(void)
{
	static enum { DARK = 0, LIGHT = 1 } led = DARK;
	int state_timeout = 0;

	if (led == DARK) {
		power_led_set_duty(0);
		state_timeout = LED_DARK_TIME;
		led = LIGHT;
	} else {
		power_led_set_duty(100);
		state_timeout = LED_LIGHT_TIME;
		led = DARK;
	}
	return state_timeout;
}

void power_led_task(void)
{
	while (1) {
		int state_timeout = -1;

		switch (led_state) {
		case POWERLED_STATE_ON:
			/*
			 * "ON" implies driving the LED using the PWM with a
			 * duty duty cycle of 100%. This produces a softer
			 * brightness than setting the GPIO to solid ON.
			 */
			power_led_use_pwm();
			power_led_set_duty(100);
			state_timeout = -1;
			break;
		case POWERLED_STATE_OFF:
			/* Reconfigure GPIO to disable the LED */
			power_led_use_pwm();
			power_led_set_duty(0);
			state_timeout = -1;
			break;
		case POWERLED_STATE_SUSPEND:
			/* Drive using PWM with variable duty cycle */
			power_led_use_pwm();
			state_timeout = power_led_step();
			break;
		default:
			break;
		}

		task_wait_event(state_timeout);
	}
}

#define CONFIG_CMD_POWERLED
#ifdef CONFIG_CMD_POWERLED
static int command_powerled(int argc, char **argv)
{
	enum powerled_state state;

	if (argc != 2)
		return EC_ERROR_INVAL;

	if (!strcasecmp(argv[1], "off"))
		state = POWERLED_STATE_OFF;
	else if (!strcasecmp(argv[1], "on"))
		state = POWERLED_STATE_ON;
	else if (!strcasecmp(argv[1], "suspend"))
		state = POWERLED_STATE_SUSPEND;
	else
		return EC_ERROR_INVAL;

	powerled_set_state(state);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powerled, command_powerled,
		"[off | on | suspend]",
		"Change power LED state",
		NULL);
#endif
#endif
