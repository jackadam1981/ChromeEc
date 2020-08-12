/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Button module for Chrome EC */

#include "atomic.h"
#include "button.h"
#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "host_command.h"
#include "hooks.h"
#include "keyboard_protocol.h"
#include "led_common.h"
#include "power_button.h"
#include "system.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/* Console output macro */
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ## args)

struct button_state_t {
	uint64_t debounce_time;
	int debounced_pressed;
};

static struct button_state_t __bss_slow state[BUTTON_COUNT];

static uint64_t __bss_slow next_deferred_time;

/*
 * Whether a button is currently pressed.
 */
static int raw_button_pressed(const struct button_config *button)
{
	int physical_value = 0;
	int simulated_value = 0;
	if (!(button->flags & BUTTON_FLAG_DISABLED)) {
		physical_value = (!!gpio_get_level(button->gpio) ==
				!!(button->flags & BUTTON_FLAG_ACTIVE_HIGH));
	}

	return (simulated_value || physical_value);
}

static void button_reset(enum button button_type,
	const struct button_config *button)
{
	state[button_type].debounced_pressed = raw_button_pressed(button);
	state[button_type].debounce_time = 0;
	gpio_enable_interrupt(button->gpio);
}

/*
 * Button initialization.
 */
void button_init(void)
{
	int i;

	CPRINTS("init buttons");
	next_deferred_time = 0;
	for (i = 0; i < BUTTON_COUNT; i++)
		button_reset(i, &buttons[i]);
}

#ifdef CONFIG_BUTTONS_RUNTIME_CONFIG
int button_reassign_gpio(enum button button_type, enum gpio_signal gpio)
{
	if (button_type >= BUTTON_COUNT)
		return EC_ERROR_INVAL;

	/* Disable currently assigned interrupt */
	gpio_disable_interrupt(buttons[button_type].gpio);

	/* Reconfigure GPIO and enable the new interrupt */
	buttons[button_type].gpio = gpio;
	button_reset(button_type, &buttons[button_type]);

	return EC_SUCCESS;
}

int button_disable_gpio(enum button button_type)
{
	if (button_type >= BUTTON_COUNT)
		return EC_ERROR_INVAL;

	/* Disable GPIO interrupt */
	gpio_disable_interrupt(buttons[button_type].gpio);
	/* Mark button as disabled */
	buttons[button_type].flags |= BUTTON_FLAG_DISABLED;

	return EC_SUCCESS;
}
#endif


/*
 * Handle debounced button changing state.
 */

static void button_change_deferred(void);
DECLARE_DEFERRED(button_change_deferred);

static void button_change_deferred(void)
{
	int i;
	int new_pressed;
	uint64_t soonest_debounce_time = 0;
	uint64_t time_now = get_time().val;

	for (i = 0; i < BUTTON_COUNT; i++) {
		/* Skip this button if we are not waiting to debounce */
		if (state[i].debounce_time == 0)
			continue;

		if (state[i].debounce_time <= time_now) {
			/* Check if the state has changed */
			new_pressed = raw_button_pressed(&buttons[i]);
			if (state[i].debounced_pressed != new_pressed) {
				state[i].debounced_pressed = new_pressed;
				CPRINTS("Button '%s' was %s",
					buttons[i].name, new_pressed ?
					"pressed" : "released");
			}

			/* Clear the debounce time to stop checking it */
			state[i].debounce_time = 0;
		} else {
			/*
			 * Make sure the next deferred call happens on or before
			 * each button needs it.
			 */
			soonest_debounce_time = (soonest_debounce_time == 0) ?
				state[i].debounce_time :
				MIN(soonest_debounce_time,
				    state[i].debounce_time);
		}
	}

	if (soonest_debounce_time != 0) {
		next_deferred_time = soonest_debounce_time;
		hook_call_deferred(&button_change_deferred_data,
				   next_deferred_time - time_now);
	}
}

/*
 * Handle a button interrupt.
 */
void button_interrupt(enum gpio_signal signal)
{
	int i;
	uint64_t time_now = get_time().val;

	for (i = 0; i < BUTTON_COUNT; i++) {
		if (buttons[i].gpio != signal ||
		    (buttons[i].flags & BUTTON_FLAG_DISABLED))
			continue;

		state[i].debounce_time = time_now + buttons[i].debounce_us;
		if (next_deferred_time <= time_now ||
		    next_deferred_time > state[i].debounce_time) {
			next_deferred_time = state[i].debounce_time;
			hook_call_deferred(&button_change_deferred_data,
					   next_deferred_time - time_now);
		}
		break;
	}
}


#ifndef CONFIG_BUTTONS_RUNTIME_CONFIG
const struct button_config buttons[BUTTON_COUNT] = {
#else
struct button_config buttons[BUTTON_COUNT] = {
#endif
	[BUTTON_ENTER] = {
		.name = "Enter",
		.gpio = GPIO_USER_BUTTON_ENTER,
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_UP] = {
		.name = "Up",
		.gpio = GPIO_USER_BUTTON_UP,
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_DOWN] = {
		.name = "Down",
		.gpio = GPIO_USER_BUTTON_DOWN,
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},
};
