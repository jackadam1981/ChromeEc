/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "board_buttons.h"
#include "button.h"
#include "chipset.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_protocol.h"
#include "led_common.h"
#include "mkbp_input_devices.h"
#include "system.h"
#include "tablet_mode.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

struct button_state_t {
	uint64_t debounce_time;
	int debounced_pressed;
};

static struct button_state_t state[BOARD_BUTTON_COUNT];

static uint64_t next_deferred_time;

/* Bitmask to keep track of simulated state of each button.
 * Bit numbers are aligned to enum button.
 */
static atomic_t sim_button_state;

/*
 * Flip state of associated button type in sim_button_state bitmask.
 * In bitmask, if bit is 1, button is pressed.  If bit is 0, button is
 * released.
 *
 * Returns the appropriate GPIO value based on table below:
 * +----------+--------+--------+
 * |  state   | active | return |
 * +----------+--------+--------+
 * | pressed  |  high  |   1    |
 * | pressed  |  low   |   0    |
 * | released |  high  |   0    |
 * | released |  low   |   1    |
 * +----------+--------+--------+
 */
static int simulated_button_pressed(const struct board_button_config *button)
{
	return !!((uint32_t)sim_button_state & BIT(button->type));
}

/*
 * Whether a button is currently pressed.
 */
static int raw_button_pressed(const struct board_button_config *button)
{
	int physical_value = 0;
	int simulated_value = 0;
	if (!(button->flags & BUTTON_FLAG_DISABLED)) {
		if (IS_ENABLED(CONFIG_ADC_BUTTONS) &&
		    button_is_adc_detected(button->gpio)) {
			physical_value = adc_to_physical_value(button->gpio);
		} else {
			physical_value =
				(!!gpio_get_level(button->gpio) ==
				 !!(button->flags & BUTTON_FLAG_ACTIVE_HIGH));
		}
		simulated_value = simulated_button_pressed(button);
	}

	return (simulated_value || physical_value);
}

static void button_reset(enum button button_type,
			 const struct board_button_config *button)
{
	state[button_type].debounced_pressed = raw_button_pressed(button);
	state[button_type].debounce_time = 0;
	gpio_enable_interrupt(button->gpio);
}

/*
 * Button initialization.
 */
void board_button_init(void)
{
	int i;

	LOG_INF("init board buttons");
	next_deferred_time = 0;
	for (i = 0; i < BOARD_BUTTON_COUNT; i++)
		button_reset(i, &board_buttons[i]);
}

/*
 * Handle debounced button changing state.
 */

static void board_button_change_deferred(void);
DECLARE_DEFERRED(board_button_change_deferred);

static void board_button_change_deferred(void)
{
	int i;
	int new_pressed;
	uint64_t soonest_debounce_time = 0;
	uint64_t time_now = get_time().val;

	for (i = 0; i < BOARD_BUTTON_COUNT; i++) {
		/* Skip this button if we are not waiting to debounce */
		if (state[i].debounce_time == 0)
			continue;

		if (state[i].debounce_time <= time_now) {
			/* Check if the state has changed */
			new_pressed = raw_button_pressed(&board_buttons[i]);
			if (state[i].debounced_pressed != new_pressed) {
				state[i].debounced_pressed = new_pressed;
				LOG_INF("Button '%s' was %s",
					board_buttons[i].name,
					new_pressed ? "pressed" : "released");

				if (i == BUTTON_ENTER) {
					/*
					 * BUTTON_ENTER is for capture image
					 * when camera app is open in tablet
					 * mode.
					 */
					if (new_pressed) {
						tablet_mode_set_override(
							TABLET_MODE_FORCE_TABLET);
					}

					mkbp_button_update(
						buttons[BUTTON_VOLUME_UP].type,
						new_pressed);
				} else {
					/*
					 * The other buttons sent scancode to
					 * implement functions. To prevent OS
					 * skip scancode in tablet mode, force
					 * clamshell mode when the button is
					 * pressed.
					 */
					if (new_pressed) {
						tablet_mode_set_override(
							TABLET_MODE_FORCE_CLAMSHELL);
					}

					keyboard_update_button(
						board_buttons[i].type,
						new_pressed);
				}
			}

			/* Clear the debounce time to stop checking it */
			state[i].debounce_time = 0;
		} else {
			/*
			 * Make sure the next deferred call happens on or before
			 * each button needs it.
			 */
			soonest_debounce_time =
				(soonest_debounce_time == 0) ?
					state[i].debounce_time :
					MIN(soonest_debounce_time,
					    state[i].debounce_time);
		}
	}

	if (soonest_debounce_time != 0) {
		next_deferred_time = soonest_debounce_time;
		hook_call_deferred(&board_button_change_deferred_data,
				   next_deferred_time - time_now);
	}
}

static atomic_val_t pending_irqs;

/* bottom half of irq handler */
void board_button_irq_handler(void)
{
	uint64_t time_now = get_time().val;
	int irqs = atomic_clear(&pending_irqs);

	for (int i = 0; i < BOARD_BUTTON_COUNT; i++) {
		if ((irqs & BIT(i)) == 0 ||
		    (board_buttons[i].flags & BUTTON_FLAG_DISABLED))
			continue;

		state[i].debounce_time =
			time_now + board_buttons[i].debounce_us;
		if (next_deferred_time <= time_now ||
		    next_deferred_time > state[i].debounce_time) {
			next_deferred_time = state[i].debounce_time;
			hook_call_deferred(&board_button_change_deferred_data,
					   next_deferred_time - time_now);
		}
	}
}
DECLARE_DEFERRED(board_button_irq_handler);

/*
 * Handle a button interrupt.
 */
void board_button_interrupt(enum gpio_signal signal)
{
	for (int i = 0; i < BOARD_BUTTON_COUNT; i++) {
		if (board_buttons[i].gpio != signal ||
		    (board_buttons[i].flags & BUTTON_FLAG_DISABLED))
			continue;

		atomic_or(&pending_irqs, BIT(i));
		hook_call_deferred(&board_button_irq_handler_data, 0);
		break;
	}
}

static int button_present(enum keyboard_button_type type)
{
	int i;

	for (i = 0; i < BOARD_BUTTON_COUNT; i++)
		if (board_buttons[i].type == type)
			break;

	return i;
}

static void button_interrupt_simulate(int button)
{
	board_button_interrupt(board_buttons[button].gpio);
}

static void simulate_button_release_deferred(void)
{
	int button_idx;

	/* Release the button */
	for (button_idx = 0; button_idx < BOARD_BUTTON_COUNT; button_idx++) {
		/* Check state for button pressed */
		if ((uint32_t)sim_button_state &
		    BIT(board_buttons[button_idx].type)) {
			/* Set state of the button as released */
			atomic_clear_bits(&sim_button_state,
					  BIT(board_buttons[button_idx].type));

			button_interrupt_simulate(button_idx);
		}
	}
}
DECLARE_DEFERRED(simulate_button_release_deferred);

static void simulate_button(uint32_t button_mask, int press_ms)
{
	int button_idx;

	/* Press the button */
	for (button_idx = 0; button_idx < BOARD_BUTTON_COUNT; button_idx++) {
		if (button_mask & BIT(button_idx)) {
			/* Set state of the button as pressed */
			atomic_or(&sim_button_state,
				  BIT(board_buttons[button_idx].type));

			button_interrupt_simulate(button_idx);
		}
	}

	/* Defer the button release for specified duration */
	hook_call_deferred(&simulate_button_release_deferred_data,
			   press_ms * 1000);
}

static int console_command_button(int argc, const char **argv)
{
	int press_ms = 50;
	char *e;
	int argv_idx;
	int button = BOARD_BUTTON_COUNT;
	uint32_t button_mask = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	for (argv_idx = 1; argv_idx < argc; argv_idx++) {
		if (!strcasecmp(argv[argv_idx], "enter"))
			button = button_present(KEYBOARD_BUTTON_ENTER);
		else if (!strcasecmp(argv[argv_idx], "back"))
			button = button_present(KEYBOARD_BUTTON_BACK);
		else if (!strcasecmp(argv[argv_idx], "overview"))
			button = button_present(KEYBOARD_BUTTON_OVERVIEW);
		else if (!strcasecmp(argv[argv_idx], "launcher"))
			button = button_present(KEYBOARD_BUTTON_LAUNCHER);
		else {
			/* If last parameter check if it is an integer. */
			if (argv_idx == argc - 1) {
				press_ms = strtoi(argv[argv_idx], &e, 0);
				/* If integer, break out of the loop. */
				if (!*e)
					break;
			}
			button = BOARD_BUTTON_COUNT;
		}

		if (button == BOARD_BUTTON_COUNT)
			return EC_ERROR_PARAM1 + argv_idx - 1;

		button_mask |= BIT(button);
	}

	if (!button_mask)
		return EC_SUCCESS;

	simulate_button(button_mask, press_ms);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(board_button, console_command_button,
			"enter|back|overview|launcher msec",
			"Simulate button press");

const struct board_button_config board_buttons[BOARD_BUTTON_COUNT] = {
	[BUTTON_ENTER] = {
		.name = "Enter",
		.type = KEYBOARD_BUTTON_ENTER,
		.gpio = GPIO_SIGNAL(DT_NODELABEL(gpio_enter_btn_odl)),
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_BACK] = {
		.name = "Back",
		.type = KEYBOARD_BUTTON_BACK,
		.gpio = GPIO_SIGNAL(DT_NODELABEL(gpio_back_btn_odl)),
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_OVERVIEW] = {
		.name = "Overview",
		.type = KEYBOARD_BUTTON_OVERVIEW,
		.gpio = GPIO_SIGNAL(DT_NODELABEL(gpio_overview_btn_odl)),
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	},

	[BUTTON_LAUNCHER] = {
		.name = "Launcher",
		.type = KEYBOARD_BUTTON_LAUNCHER,
		.gpio = GPIO_SIGNAL(DT_NODELABEL(gpio_launcher_btn_odl)),
		.debounce_us = BUTTON_DEBOUNCE_US,
		.flags = 0,
	}
};
