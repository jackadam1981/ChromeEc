/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Meowth base detection code.
 *
 * Meowth has two analog detection pins with which it monitors to determine the
 * base status: the attach, and detach pins.
 *
 * When the voltages cross a certain threshold, after some debouncing, the base
 * is deemed connected.  Meowth then applies the base power and monitors for
 * power faults from the eFuse as well as base disconnection.  Similarly, once
 * the voltages cross a different threshold, after some debouncing, the base is
 * deemed disconnected.  At this point, Meowth disables the base power.
 */

#include "adc.h"
#include "base_detect.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "tablet_mode.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define DEFAULT_POLL_TIMEOUT_US (250 * MSEC)
#define DEBOUNCE_TIMEOUT_US (20 * MSEC)

/* TODO(aaboagye): Verify these values. */
#define ATTACH_MIN_MV 300
#define ATTACH_MAX_MV 800

#define DETACH_MIN_MV 0
#define DETACH_MAX_MV 100

const struct base_det_cfg base_pin_cfg = {
	.attach_pin = ADC_BASE_ATTACH,
	.detach_pin = ADC_BASE_DETACH,
};
static int debug;
static enum base_detect_state state = BASE_DETACHED;
static int timeout = DEFAULT_POLL_TIMEOUT_US;

static void base_detect_changed(void)
{
	switch (state) {
	case BASE_DETACHED:
		/* Indicate that we are in tablet mode. */
		tablet_set_mode(1);

		/*
		 * Disable power fault interrupt.  It will read low when base
		 * power is removed.
		 */
		gpio_disable_interrupt(GPIO_BASE_PWR_FLT_L);
		/* Now, remove power to the base. */
		gpio_set_level(GPIO_BASE_PWR_EN, 0);
		break;

	case BASE_ATTACHED:
		/*
		 * Indicate that we are no longer in tablet mode.  Note, this
		 * simple logic may suffice for now, but we may have to revisit
		 * this. TODO(b/73133611).
		 */
		tablet_set_mode(0);

		/* Apply power to the base. */
		gpio_set_level(GPIO_BASE_PWR_EN, 1);
		/* Monitor for base power faults. */
		gpio_enable_interrupt(GPIO_BASE_PWR_FLT_L);
		break;

	default:
		break;
	};
}

enum base_detect_state base_get_detect_state(void)
{
	return state;
}

static int base_seems_attached(int attach_pin_mv, int detach_pin_mv)
{
	/* We can't tell if we don't have good readings. */
	if (attach_pin_mv == ADC_READ_ERROR ||
	    detach_pin_mv == ADC_READ_ERROR)
		return 0;

	if (gpio_get_level(GPIO_BASE_PWR_EN))
		return (attach_pin_mv >= 2800) && (detach_pin_mv >= 2);
	else
		return (attach_pin_mv <= ATTACH_MAX_MV) &&
			(attach_pin_mv >= ATTACH_MIN_MV) &&
			(detach_pin_mv <= 5);
}

static int base_seems_detached(int attach_pin_mv, int detach_pin_mv)
{
	/* We can't tell if we don't have good readings. */
	if (attach_pin_mv == ADC_READ_ERROR ||
	    detach_pin_mv == ADC_READ_ERROR)
		return 0;

	return (attach_pin_mv >= 2300) && (detach_pin_mv <= 10);
}

static void read_pin(int *reading, enum adc_channel pin)
{
	int read_val = adc_read_channel(pin);

	if (debug)
		CPRINTS("BD: %sach: %dmV",
			pin == base_pin_cfg.attach_pin ? "att" : "det",
			read_val);

	*reading = read_val;
}

static void set_state(enum base_detect_state new_state)
{
	if (new_state != state) {
		CPRINTS("BD: st%d", new_state);
		state = new_state;
	}
}

static void base_detect_deferred(void);
DECLARE_DEFERRED(base_detect_deferred);
static void base_detect_deferred(void)
{
	int attach_reading;
	int detach_reading;

	watchdog_reload();
	read_pin(&attach_reading, base_pin_cfg.attach_pin);
	read_pin(&detach_reading, base_pin_cfg.detach_pin);

	switch (state) {
	case BASE_DETACHED:
		/* Check to see if a base may be attached. */
		if (base_seems_attached(attach_reading, detach_reading)) {
			timeout = DEBOUNCE_TIMEOUT_US;
			set_state(BASE_ATTACHED_DEBOUNCE);
		}
		break;

	case BASE_ATTACHED_DEBOUNCE:
		/* Check to see if it's still attached. */
		if (base_seems_attached(attach_reading, detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_ATTACHED);
			base_detect_changed();
		} else if (base_seems_detached(attach_reading,
					       detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_DETACHED);
		}
		break;

	case BASE_ATTACHED:
		/* Check to see if a base may be detached. */
		if (base_seems_detached(attach_reading, detach_reading)) {
			timeout = DEBOUNCE_TIMEOUT_US;
			set_state(BASE_DETACHED_DEBOUNCE);
		}
		break;

	case BASE_DETACHED_DEBOUNCE:
		/* Check to see if a base is still detached. */
		if (base_seems_detached(attach_reading, detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_DETACHED);
			base_detect_changed();
		} else if (base_seems_attached(attach_reading,
					       detach_reading)) {
			timeout = DEFAULT_POLL_TIMEOUT_US;
			set_state(BASE_ATTACHED);
		}
		break;
		/* TODO(aaboagye): do you want to add an interrupt? */

	default:
		break;
	};

	/* Check again in the appropriate time. */
	hook_call_deferred(&base_detect_deferred_data, timeout);
};
DECLARE_HOOK(HOOK_INIT, base_detect_deferred, HOOK_PRIO_DEFAULT);

static uint8_t base_power_on_attempts;
static void clear_base_power_on_attempts_deferred(void)
{
	base_power_on_attempts = 0;
}
DECLARE_DEFERRED(clear_base_power_on_attempts_deferred);

static void check_and_reapply_base_power_deferred(void)
{
	if (base_get_detect_state() != BASE_ATTACHED)
		return;

	if (base_power_on_attempts < 3) {
		CPRINTS("Reapply base power");
		gpio_set_level(GPIO_BASE_PWR_EN, 1);
		base_power_on_attempts++;

		hook_call_deferred(&clear_base_power_on_attempts_deferred_data,
				   SECOND);
	}

}
DECLARE_DEFERRED(check_and_reapply_base_power_deferred);

void base_pwr_fault(enum gpio_signal s)
{
	/* Inverted because active low. */
	int fault_detected = !gpio_get_level(GPIO_BASE_PWR_FLT_L);

	if (fault_detected) {
		/* Turn off base power. */
		CPRINTS("Base Power Fault!");
		gpio_set_level(GPIO_BASE_PWR_EN, 0);

		/*
		 * Try and apply power in a bit if maybe it was just a temporary
		 * condition.
		 */
		hook_call_deferred(&check_and_reapply_base_power_deferred_data,
				   15 * MSEC);
	}
}

static int command_basedetectdebug(int argc, char **argv)
{
	if (argc > 1)
		if (!parse_bool(argv[1], &debug))
			return EC_ERROR_PARAM1;

	CPRINTS("BD: st%d", state);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(basedet, command_basedetectdebug, "[enable|disable]",
			"Enable/Disable base detection debug info.");
