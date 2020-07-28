/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Power button module for Chrome EC */

#include "button.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SWITCH, outstr)
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ## args)

/* By default the power button is active low */
#ifndef CONFIG_POWER_BUTTON_FLAGS
#define CONFIG_POWER_BUTTON_FLAGS 0
#endif

static int debounced_power_pressed;  /* Debounced power button state */
static int simulate_power_pressed;
static timestamp_t debounced_at;     /* Time debounce was (or will be) done */

static const struct button_config power_button = {
	.name = "power button",
	.gpio = GPIO_POWER_BUTTON_L,
	.debounce_us = BUTTON_DEBOUNCE_US,
	.flags = CONFIG_POWER_BUTTON_FLAGS,
};

int power_button_signal_asserted(void)
{
	return !!(gpio_get_level(power_button.gpio)
		== (power_button.flags & BUTTON_FLAG_ACTIVE_HIGH) ? 1 : 0);
}

/**
 * Get raw power button signal state.
 *
 * @return 1 if power button is pressed, 0 if not pressed.
 */
static int raw_power_button_pressed(void)
{
	if (simulate_power_pressed)
		return 1;

#ifndef CONFIG_POWER_BUTTON_IGNORE_LID
	/*
	 * Always indicate power button released if the lid is closed.
	 * This prevents waking the system if the device is squashed enough to
	 * press the power button through the closed lid.
	 */
	if (!lid_is_open())
		return 0;
#endif

	return power_button_signal_asserted();
}

int power_button_is_pressed(void)
{
	return debounced_power_pressed;
}

static void power_button_wait_for_change(int timeout_us)
{
	/* Copy debounced_at to avoid being influenced by back-to-back IRQs */
	timestamp_t t1 = debounced_at;
	timestamp_t now = get_time();

	if (t1.val < now.val) {
		/* Not debouncing. Wait for HOOK_POWER_BUTTON_CHANGE. */
		task_wait_event(timeout_us);
	} else if (timeout_us < 0 || t1.val < now.val + timeout_us) {
		/* Debounce will be done before timeout. Let debounce finish. */
		usleep(t1.val - now.val);
	} else {
		/* We'll timeout before debounce is done. */
		usleep(timeout_us);
	}
}

int power_button_wait_for_release(int timeout_us)
{
	timestamp_t deadline;

	deadline.val = get_time().val + timeout_us;

	do {
		power_button_wait_for_change(timeout_us);
		if (timeout_us >= 0) {
			timeout_us = deadline.val - get_time().val;
			if (timeout_us < 0) {
				CPRINTS("power button not released in time");
				return EC_ERROR_TIMEOUT;
			}
		}
	} while (power_button_is_pressed());

	CPRINTS("power button released");

	return EC_SUCCESS;
}

/**
 * Handle power button initialization.
 */
static void power_button_init(void)
{
	if (raw_power_button_pressed())
		debounced_power_pressed = 1;

	/* Enable interrupts, now that we've initialized */
	gpio_enable_interrupt(power_button.gpio);
}
DECLARE_HOOK(HOOK_INIT, power_button_init, HOOK_PRIO_INIT_POWER_BUTTON);

#ifdef CONFIG_POWER_BUTTON_INIT_IDLE
/*
 * Set/clear AP_IDLE flag. It's set when the system gracefully shuts down and
 * it's cleared when the system boots up. The result is the system tries to
 * go back to the previous state upon AC plug-in. If the system uncleanly
 * shuts down, it boots immediately. If the system shuts down gracefully,
 * it'll stay at S5 and wait for power button press.
 */
static void pb_chipset_startup(void)
{
	chip_save_reset_flags(chip_read_reset_flags() & ~EC_RESET_FLAG_AP_IDLE);
	system_clear_reset_flags(EC_RESET_FLAG_AP_IDLE);
	CPRINTS("Cleared AP_IDLE flag");
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, pb_chipset_startup, HOOK_PRIO_DEFAULT);

static void pb_chipset_shutdown(void)
{
	chip_save_reset_flags(chip_read_reset_flags() | EC_RESET_FLAG_AP_IDLE);
	system_set_reset_flags(EC_RESET_FLAG_AP_IDLE);
	CPRINTS("Saved AP_IDLE flag");
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, pb_chipset_shutdown,
	     /*
	      * Slightly higher than handle_pending_reboot because
	      * it may clear AP_IDLE flag.
	      */
	     HOOK_PRIO_DEFAULT - 1);
#endif

/**
 * Handle debounced power button changing state.
 */
static void power_button_change_deferred(void)
{
	const int new_pressed = raw_power_button_pressed();

	/* Re-enable keyboard scanning if power button is no longer pressed */
	if (!new_pressed)
		keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);

	/* If power button hasn't changed state, nothing to do */
	if (new_pressed == debounced_power_pressed)
		return;

	debounced_power_pressed = new_pressed;

	CPRINTS("%s %s",
		power_button.name, new_pressed ? "pressed" : "released");

	/* Call hooks */
	hook_notify(HOOK_POWER_BUTTON_CHANGE);

	/* Notify host if power button has been pressed */
	if (new_pressed)
		host_set_single_event(EC_HOST_EVENT_POWER_BUTTON);
}
DECLARE_DEFERRED(power_button_change_deferred);

void power_button_interrupt(enum gpio_signal signal)
{
	/*
	 * If power button is pressed, disable the matrix scan as soon as
	 * possible to reduce the risk of false-reboot triggered by those keys
	 * on the same column with refresh key.
	 */
	if (raw_power_button_pressed())
		keyboard_scan_enable(0, KB_SCAN_DISABLE_POWER_BUTTON);

	/* Reset power button debounce time */
	debounced_at.val = get_time().val + power_button.debounce_us;
	hook_call_deferred(&power_button_change_deferred_data,
			   power_button.debounce_us);
}

/*****************************************************************************/
/* Console commands */

static int command_powerbtn(int argc, char **argv)
{
	int ms = 200;  /* Press duration in ms */
	char *e;

	if (argc > 1) {
		ms = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
	}

	ccprintf("Simulating %d ms %s press.\n", ms, power_button.name);
	simulate_power_pressed = 1;
	debounced_at = get_time();
	hook_call_deferred(&power_button_change_deferred_data, 0);

	if (ms > 0)
		msleep(ms);

	ccprintf("Simulating %s release.\n", power_button.name);
	simulate_power_pressed = 0;
	debounced_at = get_time();
	hook_call_deferred(&power_button_change_deferred_data, 0);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(powerbtn, command_powerbtn,
			"[msec]",
			"Simulate power button press");

