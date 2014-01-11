/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Button module for Chrome EC */

#include "button.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"
#include "util.h"

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_SWITCH, format, ## args)

#define MAXUINT ~0x00

struct button_state_t {
	int debounced_pressed;
	uint64_t debounce_time;
};

static struct button_state_t state[CONFIG_BUTTON_COUNT];

static uint64_t next_deferred_time;

/*
 * Whether a button is currently pressed.
 */
static int raw_button_pressed(const struct button_config *button)
{
	int raw_value = gpio_get_level(button->gpio);

	if (button->active_level == BUTTON_ACTIVE_HIGH)
		return raw_value;

	return raw_value ? 0 : 1;
}

/*
 * Button initialization.
 */
static void button_init(void)
{
	int i;

	next_deferred_time = 0;
	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
		state[i].debounced_pressed = raw_button_pressed(&buttons[i]);
		gpio_enable_interrupt(buttons[i].gpio);
	}
}
DECLARE_HOOK(HOOK_INIT, button_init, HOOK_PRIO_DEFAULT);

/*
 * Handle debounced button changing state.
 */
static void button_change_deferred(void)
{
	int i;
	int new_pressed;
	uint64_t time_now = get_time().val;
	uint64_t soonest_debounce_time = MAXUINT;

	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
		/* Skip this button if we are not waiting to debounce */
		if (state[i].debounce_time == 0)
			continue;

		if (state[i].debounce_time < time_now) {
			/* Check if the state has changed */
			new_pressed = raw_button_pressed(&buttons[i]);
			if (new_pressed != state[i].debounced_pressed) {
				state[i].debounced_pressed = new_pressed;
				button_state_changed(buttons[i].type,
					new_pressed);
				CPRINTF("[%T Button '%s' was %s]\n",
					buttons[i].name, new_pressed ?
					"pressed" : "released");
			}

			/* Clear the debounce time to stop checking it */
			state[i].debounce_time = 0;
		} else {
			/* Keep track of the next time to check a button */
			soonest_debounce_time = MIN(state[i].debounce_time,
						    soonest_debounce_time);
		}
	}

	if (soonest_debounce_time != MAXUINT)
		hook_call_deferred(button_change_deferred,
				   soonest_debounce_time - time_now);
}
DECLARE_DEFERRED(button_change_deferred);

/*
 * Handle a button interrupt.
 */
void button_interrupt(enum gpio_signal signal)
{
	int i;
	uint64_t time_now = get_time().val;

	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
		if (buttons[i].gpio != signal)
			continue;

		state[i].debounce_time = time_now + buttons[i].debounce_us;
		if (next_deferred_time < time_now) {
			next_deferred_time = state[i].debounce_time;
			hook_call_deferred(button_change_deferred,
					   next_deferred_time - time_now);
		}
		break;
	}
}
