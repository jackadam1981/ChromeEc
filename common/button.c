/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Button module for Chrome EC */

#include "button.h"
#include "chipset.h"
#include "common.h"
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

static struct button_state_t __bss_slow state[CONFIG_BUTTON_COUNT];

static uint64_t __bss_slow next_deferred_time;

#ifdef CONFIG_CMD_BUTTON
static int siml_btn_presd;

static int simulated_button_pressed(void)
{
	static int button = 1;

	button = !button;
	return button;
}
#endif

/*
 * Whether a button is currently pressed.
 */
static int raw_button_pressed(const struct button_config *button)
{
	int raw_value =
#ifdef CONFIG_CMD_BUTTON
			siml_btn_presd ?
			simulated_button_pressed() :
#endif
			gpio_get_level(button->gpio);

	return button->flags & BUTTON_FLAG_ACTIVE_HIGH ?
				       raw_value : !raw_value;
}

#ifdef CONFIG_BUTTON_RECOVERY

#ifdef CONFIG_LED_COMMON
static void button_blink_hw_reinit_led(void)
{
	int led_state = LED_STATE_ON;
	timestamp_t deadline;
	timestamp_t now = get_time();

	/* Blink LED for 3 seconds. */
	deadline.val = now.val + (3 * SECOND);

	while (!timestamp_expired(deadline, &now)) {
		led_control(EC_LED_ID_RECOVERY_HW_REINIT_LED, led_state);
		led_state = !led_state;
		watchdog_reload();
		msleep(100);
		now = get_time();
	}

	/* Reset LED to default state. */
	led_control(EC_LED_ID_RECOVERY_HW_REINIT_LED, LED_STATE_RESET);
}
#endif

/*
 * Whether recovery button (or combination of equivalent buttons) is pressed
 */
static int is_recovery_button_pressed(void)
{
	int i;
	for (i = 0; i < recovery_buttons_count; i++) {
		if (!raw_button_pressed(recovery_buttons[i]))
			return 0;
	}
	return 1;
}

/*
 * If the EC is reset and recovery is requested, then check if HW_REINIT is
 * requested as well. Since the EC reset occurs after volup+voldn+power buttons
 * are held down for 10 seconds, check the state of these buttons for 20 more
 * seconds. If they are still held down all this time, then set host event to
 * indicate HW_REINIT is requested. Also, make sure watchdog is reloaded in
 * order to prevent watchdog from resetting the EC.
 */
static void button_check_hw_reinit_required(void)
{
	timestamp_t deadline;
	timestamp_t now = get_time();

	deadline.val = now.val + (20 * SECOND);

	CPRINTS("Checking for HW_REINIT request");

	while (!timestamp_expired(deadline, &now)) {
		if (!is_recovery_button_pressed() ||
		    !power_button_signal_asserted()) {
			CPRINTS("No HW_REINIT request");
			return;
		}
		now = get_time();
		watchdog_reload();
	}

	CPRINTS("HW_REINIT requested");
	host_set_single_event(EC_HOST_EVENT_KEYBOARD_RECOVERY_HW_REINIT);

#ifdef CONFIG_LED_COMMON
	button_blink_hw_reinit_led();
#endif
}

static int is_recovery_boot(void)
{
	if (system_jumped_to_this_image())
		return 0;
	if (!(system_get_reset_flags() &
	    (RESET_FLAG_RESET_PIN | RESET_FLAG_POWER_ON)))
		return 0;
	if (!is_recovery_button_pressed())
		return 0;
	return 1;
}
#endif	/* CONFIG_BUTTON_RECOVERY */

/*
 * Button initialization.
 */
void button_init(void)
{
	int i;

	CPRINTS("init buttons");
	next_deferred_time = 0;
	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
		state[i].debounced_pressed = raw_button_pressed(&buttons[i]);
		state[i].debounce_time = 0;
		gpio_enable_interrupt(buttons[i].gpio);
	}

#ifdef CONFIG_BUTTON_RECOVERY
	if (is_recovery_boot()) {
		host_set_single_event(EC_HOST_EVENT_KEYBOARD_RECOVERY);
		button_check_hw_reinit_required();
	}
#endif
}

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

	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
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
#if defined(HAS_TASK_KEYPROTO) || defined(CONFIG_KEYBOARD_PROTOCOL_MKBP)
				keyboard_update_button(buttons[i].type,
					new_pressed);
#endif
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

	for (i = 0; i < CONFIG_BUTTON_COUNT; i++) {
		if (buttons[i].gpio != signal)
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

#ifdef CONFIG_CMD_BUTTON
static int button_present(enum keyboard_button_type type)
{
	int i;

	for (i = 0; i < CONFIG_BUTTON_COUNT; i++)
		if (buttons[i].type == type)
			break;

	return i;
}

static void button_interrupt_simulate(int button)
{
	button_interrupt(buttons[button].gpio);
	usleep(buttons[button].debounce_us >> 2);
	button_interrupt(buttons[button].gpio);
}

static int console_command_button(int argc, char **argv)
{
	int button;
	int press_ms = 50;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "vup"))
		button = button_present(KEYBOARD_BUTTON_VOLUME_UP);
	else if (!strcasecmp(argv[1], "vdown"))
		button = button_present(KEYBOARD_BUTTON_VOLUME_DOWN);
	else
		return EC_ERROR_PARAM1;

	if (button == CONFIG_BUTTON_COUNT)
		return EC_ERROR_PARAM1;

	if (argc > 2) {
		press_ms = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;
	}

	siml_btn_presd = 1;

	/* Press the button */
	button_interrupt_simulate(button);

	/* Hold the button */
	msleep(press_ms);

	/* Release the button */
	button_interrupt_simulate(button);

	/* Wait till button processing is finished */
	msleep(100);

	siml_btn_presd = 0;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(button, console_command_button,
			"vup|vdown msec",
			"Simulate button press");
#endif

#ifdef CONFIG_EMULATED_SYSRQ

enum debug_state {
	STATE_DEBUG_NONE,
	STATE_DEBUG_CHECK,
	STATE_STAGING,
	STATE_DEBUG_MODE_ACTIVE,
	STATE_SYSRQ_PATH,
	STATE_WARM_RESET_PATH,
	STATE_SYSRQ_EXEC,
	STATE_WARM_RESET_EXEC,
};

enum debug_btn {
	DEBUG_BTN_POWER = (1 << 0),
	DEBUG_BTN_VOL_UP = (1 << 1),
	DEBUG_BTN_VOL_DN = (1 << 2),
};

#define DEBUG_TIMEOUT		(10 * SECOND)

static enum debug_state curr_debug_state = STATE_DEBUG_NONE;
static enum debug_state next_debug_state = STATE_DEBUG_NONE;
static int staging_key_mask;
static timestamp_t debug_state_deadline;
static int debug_button_hit_count;
#ifdef CONFIG_LED_COMMON
static int debug_mode_blink_led;
#endif

static int debug_button_mask(void)
{
	int mask;

	/* Get power button state */
	mask = power_button_signal_asserted() ? DEBUG_BTN_POWER : 0;

	/* Get volume up state */
	mask |= raw_button_pressed(&buttons[BUTTON_VOLUME_UP]) ?
		DEBUG_BTN_VOL_UP : 0;

	/* Get volume down state */
	mask |= raw_button_pressed(&buttons[BUTTON_VOLUME_DOWN]) ?
		DEBUG_BTN_VOL_DN : 0;

	return mask;
}

static int debug_button_pressed(int mask)
{
	return debug_button_mask() == mask;
}

static void debug_mode_transition(enum debug_state next_state)
{
	timestamp_t now = get_time();

	curr_debug_state = next_state;

	/*
	 * If user actions reached a final state (SYSRQ_EXEC / WARM_RESET_EXEC),
	 * take necessary action corresponding to button presses and immediately
	 * transition to STATE_DEBUG_NONE.
	 */
	if (curr_debug_state == STATE_SYSRQ_EXEC) {
		host_send_sysrq('x');
		curr_debug_state = STATE_DEBUG_NONE;
	} else if (curr_debug_state == STATE_WARM_RESET_EXEC) {
		chipset_reset(0);
		curr_debug_state = STATE_DEBUG_NONE;
	}

	/* If state machine reached initial state, reset all variables. */
	if (curr_debug_state == STATE_DEBUG_NONE) {
		next_debug_state = STATE_DEBUG_NONE;
		staging_key_mask = 0;
		debug_state_deadline.val = 0;
		debug_button_hit_count = 0;
#ifdef CONFIG_LED_COMMON
		if (debug_mode_blink_led) {
			led_control(EC_LED_ID_SYSRQ_DEBUG_LED, LED_STATE_RESET);
			debug_mode_blink_led = 0;
		}
#endif
		return;
	}

	/* Set deadline to 10seconds from current time. */
	debug_state_deadline.val = now.val + DEBUG_TIMEOUT;

	/*
	 * If system entered debug mode, then:
	 *  1. Start blinking LED.
	 *  2. Reset button hit count to 0.
	 */
	if (curr_debug_state == STATE_DEBUG_MODE_ACTIVE) {
#ifdef CONFIG_LED_COMMON
		debug_mode_blink_led = 1;
#endif
		debug_button_hit_count = 0;
	}
}

static int debug_mode_timeout(void)
{
	timestamp_t now = get_time();

	return timestamp_expired(debug_state_deadline, &now);
}

static void debug_mode_tick(void)
{
	int mask;
#ifdef CONFIG_LED_COMMON
	static int led_state = LED_STATE_OFF;

	if (debug_mode_blink_led) {
		led_state = !led_state;
		led_control(EC_LED_ID_SYSRQ_DEBUG_LED, led_state);
	}
#endif

	switch (curr_debug_state) {
	case STATE_DEBUG_NONE:
		/*
		 * If user pressed Vup+Vdn, check for next 10 seconds to see if
		 * user keeps holding the keys.
		 */
		if (debug_button_pressed(DEBUG_BTN_VOL_UP | DEBUG_BTN_VOL_DN))
			debug_mode_transition(STATE_DEBUG_CHECK);
		break;
	case STATE_DEBUG_CHECK:
		/*
		 * If no key is pressed or any key combo other than Vup+Vdn is
		 * held, then quit debug check mode.
		 */
		if (!debug_button_pressed(DEBUG_BTN_VOL_UP | DEBUG_BTN_VOL_DN))
			debug_mode_transition(STATE_DEBUG_NONE);
		else if (debug_mode_timeout()) {
			/*
			 * If Vup+Vdn are held down for 10 seconds, then its
			 * time to enter debug mode.
			 */
			next_debug_state = STATE_DEBUG_MODE_ACTIVE;
			staging_key_mask = DEBUG_BTN_VOL_UP | DEBUG_BTN_VOL_DN;
			debug_mode_transition(STATE_STAGING);
		}
		break;
	case STATE_STAGING:
		mask = debug_button_mask();

		/* If no button is pressed, transition to next state. */
		if (!mask) {
			debug_mode_transition(next_debug_state);
			return;
		}

		/*
		 * Exit debug mode if:
		 *  1. Any key other than staging key is pressed or
		 *  2. Keys are stuck for > 10 seconds.
		 */
		if ((mask != staging_key_mask) || debug_mode_timeout())
			debug_mode_transition(STATE_DEBUG_NONE);

		break;
	case STATE_DEBUG_MODE_ACTIVE:
		mask = debug_button_mask();

		/*
		 * Continue in this state if button is not pressed and timeout
		 * has not occurred.
		 */
		if (!mask && !debug_mode_timeout())
			return;

		/* Exit debug mode if valid buttons are not pressed. */
		if ((mask != DEBUG_BTN_VOL_UP) && (mask != DEBUG_BTN_VOL_DN)) {
			debug_mode_transition(STATE_DEBUG_NONE);
			return;
		}

		/*
		 * Transition to STAGING state with next state set to:
		 * 1. SYSRQ_PATH     : If Vup was pressed.
		 * 2. WARM_RESET_PATH: If Vdn was pressed.
		 */
		if (mask == DEBUG_BTN_VOL_UP)
			next_debug_state = STATE_SYSRQ_PATH;
		else
			next_debug_state = STATE_WARM_RESET_PATH;

		staging_key_mask = mask;
		debug_mode_transition(STATE_STAGING);
		break;
	case STATE_SYSRQ_PATH:
		mask = debug_button_mask();

		/*
		 * Continue in this state if button is not pressed and timeout
		 * has not occurred.
		 */
		if (!mask && !debug_mode_timeout())
			return;

		/* Exit debug mode if valid buttons are not pressed. */
		if ((mask != DEBUG_BTN_VOL_UP) && (mask != DEBUG_BTN_VOL_DN)) {
			debug_mode_transition(STATE_DEBUG_NONE);
			return;
		}

		if (mask == DEBUG_BTN_VOL_UP) {
			/*
			 * In case Vup is pressed:
			 * 1. Increment button hit count.
			 * 2. If buttons hit count is greater than expected,
			 * then exit debug mode.
			 */
			debug_button_hit_count++;
			if (debug_button_hit_count == 4) {
				debug_mode_transition(STATE_DEBUG_NONE);
				return;
			}
			/*
			 * Else transition to STAGING state with next state set
			 * to SYSRQ_PATH.
			 */
			next_debug_state = STATE_SYSRQ_PATH;
		} else
			/*
			 * Else if Vdn is pressed, transition to STAGING with
			 * next state set to SYSRQ_EXEC.
			 */
			next_debug_state = STATE_SYSRQ_EXEC;
		staging_key_mask = mask;
		debug_mode_transition(STATE_STAGING);
		break;
	case STATE_WARM_RESET_PATH:
		mask = debug_button_mask();

		/*
		 * Continue in this state if button is not pressed and timeout
		 * has not occurred.
		 */
		if (!mask && !debug_mode_timeout())
			return;

		/* Exit debug mode if valid buttons are not pressed. */
		if (mask != DEBUG_BTN_VOL_UP) {
			debug_mode_transition(STATE_DEBUG_NONE);
			return;
		}

		next_debug_state = STATE_WARM_RESET_EXEC;
		staging_key_mask = DEBUG_BTN_VOL_UP;
		debug_mode_transition(STATE_STAGING);
		break;
	case STATE_SYSRQ_EXEC:
	case STATE_WARM_RESET_EXEC:
	default:
		debug_mode_transition(STATE_DEBUG_NONE);
		break;
	}
}

DECLARE_HOOK(HOOK_TICK, debug_mode_tick, HOOK_PRIO_DEFAULT);

#endif /* CONFIG_EMULATED_SYSRQ */
