/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * SDM845 SoC power sequencing module for Chrome EC
 *
 * This implements the following features:
 *
 * - Cold reset powers on the AP
 *
 *  When powered off:
 *  - Press pwron turns on the AP
 *  - Hold pwron turns on the AP, and then 8s later turns it off and leaves
 *    it off until pwron is released and pressed again
 *
 *  When powered on:
 *  - Holding pwron for 8s powers off the AP
 *  - Pressing and releasing pwron within that 8s is ignored
 *  - If POWER_GOOD is dropped by the AP, then we power the AP off
 *
 * TODO(waihong): Handle suspend/resume?
 */

#include "battery.h"
#include "chipset.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "test_util.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* masks for power signals */
#define IN_POWER_GOOD POWER_SIGNAL_MASK(SDM845_POWER_GOOD)

/* Long power key press to force shutdown */
#define DELAY_FORCE_SHUTDOWN  (8 * SECOND)

/*
 * If the power key is pressed to turn on, then held for this long, we
 * power off.
 *
 * Normal case: User releases power button and chipset_task() goes
 *    into the inner loop, waiting for next event to occur (power button
 *    press or POWER_GOOD == 0).
 */
#define DELAY_SHUTDOWN_ON_POWER_HOLD	(8 * SECOND)

/*
 * The hold time for pulling down the PMIC_WARM_RESET_H pin so that
 * the AP can entery the recovery mode (flash SPI flash from USB).
 */
#define PMIC_WARM_RESET_H_HOLD_TIME (4 * MSEC)

/* Wait for AP reset signal */
#define PMIC_WAIT_FOR_AP_RESET (1 * MSEC)

/*
 * If POWER_GOOD is lost, wait for PMIC to turn off its power completely
 * before we turn off VBAT by set_system_power(0)
 */
#define PMIC_POWER_OFF_DELAY (50 * MSEC)

/* TODO(crosbug.com/p/25047): move to HOOK_POWER_BUTTON_CHANGE */
/* 1 if the power button was pressed last time we checked */
static char power_button_was_pressed;

/* 1 if lid-open event has been detected */
static char lid_opened;

/* time where we will power off, if power button still held down */
static timestamp_t power_off_deadline;

/* force AP power on (used for recovery keypress) */
static int auto_power_on;

enum power_request_t {
	POWER_REQ_NONE,
	POWER_REQ_OFF,
	POWER_REQ_ON,

	POWER_REQ_COUNT,
};

static enum power_request_t power_request;

/**
 * Return values for check_for_power_off_event().
 */
enum power_off_event_t {
	POWER_OFF_CANCEL,
	POWER_OFF_BY_POWER_BUTTON_PRESSED,
	POWER_OFF_BY_LONG_PRESS,
	POWER_OFF_BY_POWER_GOOD_LOST,
	POWER_OFF_BY_POWER_REQ,

	POWER_OFF_EVENT_COUNT,
};

/**
 * Return values for check_for_power_on_event().
 */
enum power_on_event_t {
	POWER_ON_CANCEL,
	POWER_ON_BY_IN_POWER_GOOD,
	POWER_ON_BY_AUTO_POWER_ON,
	POWER_ON_BY_LID_OPEN,
	POWER_ON_BY_POWER_BUTTON_PRESSED,
	POWER_ON_BY_POWER_REQ_NONE,

	POWER_ON_EVENT_COUNT,
};

/* Forward declaration */
static void chipset_turn_off_power_rails(void);

/**
 * Set the system power signal.
 *
 * @param asserted	off (=0) or on (=1)
 */
static void set_system_power(int asserted)
{
	CPRINTS("set_system_power(%d)", asserted);
	/* Use the switchcap to control the whole system power */
	gpio_set_level(GPIO_SWITCHCAP_ON_L, !asserted);
}

/**
 * Get the system power signal.
 *
 * @return non-zero if the system is powered, 0 if not
 */
static int is_system_powered(void)
{
	return !gpio_get_level(GPIO_SWITCHCAP_ON_L);
}

/**
 * Get the AP power signal.
 *
 * @return non-zero if the AP is powered, 0 if not
 */
static int is_ap_powered(void)
{
	return gpio_get_level(GPIO_AP_PS_HOLD);
}

/**
 * Set the PMIC power-on, which also power-on AP.
 *
 * @param asserted	Assert (=1) or deassert (=0) the signal.  This is the
 *			logical level of the pin, not the physical level.
 */
static void set_pmic_pwron(int asserted)
{
	timestamp_t poll_deadline;
	/* Signal is active-high */
	CPRINTS("set_pmic_pwron(%d)", asserted);

	/* Power-on sequence:
	 * 1 hold down SYS_RESET_L signal which connects to RESIN_N pin of PM845
	 * 2 PM845 pulls up AP_RESET_L signal to power-on SDM845
	 * 3 SDM845 pulls up AP_PS_HOLD signal
	 * 4 wait for AP_PS_HOLD up, which may take 16 seconds timeout (for the
	 *   first boot, not yet reprogrammed to 0-second)
	 * 5 release SYS_RESET_L signal
	 * 6 wait for AP_PS_HOLD up, timeout 1 second
	 *
	 * TODO(waihong): Confirm SYS_RESET_L is the right signal to power-on.
	 * The behavior of RESIN_N pin is the same as KPDPWR_N pin?
	 */

	/* GPIO_SYS_RESET_L connects to RESIN_N pin of PMIC */
	gpio_set_level(GPIO_SYS_RESET_L, 0);

	/* TODO(waihong): Use power_wait_signals() */
	poll_deadline = get_time();
	/* TODO(waihong): Tune the timeout and declare it as a constant */
	poll_deadline.val += 16 * SECOND;
	while ((asserted ^ is_ap_powered()) &&
	       get_time().val < poll_deadline.val)
		usleep(PMIC_WAIT_FOR_AP_RESET);
	if (asserted ^ is_ap_powered()) {
		if (asserted)
			CPRINTS("AP power not ready");
		else
			CPRINTS("AP power still up");
	}

	/* Relase RESIN_N pin */
	gpio_set_level(GPIO_SYS_RESET_L, 1);
}

/**
 * Check for some event triggering the shutdown.
 *
 * It can be either a long power button press or a shutdown triggered from the
 * AP and detected by reading POWER_GOOD.
 *
 * @return non-zero if a shutdown should happen, 0 if not
 */
static int check_for_power_off_event(void)
{
	timestamp_t now;
	int pressed = 0;

	/*
	 * Check for power button press.
	 */
	if (power_button_is_pressed()) {
		pressed = POWER_OFF_BY_POWER_BUTTON_PRESSED;
	} else if (power_request == POWER_REQ_OFF) {
		power_request = POWER_REQ_NONE;
		/* return non-zero for shudown down */
		return POWER_OFF_BY_POWER_REQ;
	}

	now = get_time();
	if (pressed) {
		if (!power_button_was_pressed) {
			power_off_deadline.val = now.val + DELAY_FORCE_SHUTDOWN;
			CPRINTS("power waiting for long press %u",
				power_off_deadline.le.lo);
			/* Ensure we will wake up to check the power key */
			timer_arm(power_off_deadline, TASK_ID_CHIPSET);
		} else if (timestamp_expired(power_off_deadline, &now)) {
			power_off_deadline.val = 0;
			CPRINTS("power off after long press now=%u, %u",
				now.le.lo, power_off_deadline.le.lo);
			return POWER_OFF_BY_LONG_PRESS;
		}
	} else if (power_button_was_pressed) {
		CPRINTS("power off cancel");
		timer_cancel(TASK_ID_CHIPSET);
	}

	power_button_was_pressed = pressed;

	/* POWER_GOOD released by AP : shutdown immediately */
	if (!power_has_signals(IN_POWER_GOOD)) {
		if (power_button_was_pressed)
			timer_cancel(TASK_ID_CHIPSET);

		CPRINTS("POWER_GOOD is lost");
		return POWER_OFF_BY_POWER_GOOD_LOST;
	}

	return POWER_OFF_CANCEL;
}

static void sdm845_lid_event(void)
{
	/* Power task only cares about lid-open events */
	if (!lid_is_open())
		return;

	lid_opened = 1;
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_HOOK(HOOK_LID_CHANGE, sdm845_lid_event, HOOK_PRIO_DEFAULT);

enum power_state power_chipset_init(void)
{
	int init_power_state;
	uint32_t reset_flags = system_get_reset_flags();

	/*
	 * Force the AP shutdown unless we are doing SYSJUMP. Otherwise,
	 * the AP could stay in strange state.
	 */
	if (!(reset_flags & RESET_FLAG_SYSJUMP)) {
		CPRINTS("not sysjump; forcing AP shutdown");
		chipset_turn_off_power_rails();

		/*
		 * The warm reset triggers AP into the recovery mode (
		 * flash SPI from USB).
		 */
		chipset_reset(0);

		init_power_state = POWER_G3;
	} else {
		/* In the SYSJUMP case, we check if the AP is on */
		if (power_get_signals() & IN_POWER_GOOD) {
			CPRINTS("SOC ON");
			init_power_state = POWER_S0;
		} else {
			CPRINTS("SOC OFF");
			init_power_state = POWER_G3;
		}
	}

	/* Leave power off only if requested by reset flags */
	if (!(reset_flags & RESET_FLAG_AP_OFF) &&
	    !(reset_flags & RESET_FLAG_SYSJUMP)) {
		CPRINTS("auto_power_on set due to reset_flag 0x%x",
			system_get_reset_flags());
		auto_power_on = 1;
	}

	/*
	 * Some batteries use clock stretching feature, which requires
	 * more time to be stable. See http://crosbug.com/p/28289
	 */
	battery_wait_for_stable();

	return init_power_state;
}

/*****************************************************************************/
/* Chipset interface */

static void chipset_turn_off_power_rails(void)
{
	/* Release the power on pin, if it was asserted */
	set_pmic_pwron(0);

	/* system power off */
	/* TODO(waihong): Tune this delay or wait PS_HOLD signal */
	usleep(PMIC_POWER_OFF_DELAY);
	set_system_power(0);
}

void chipset_force_shutdown(void)
{
	chipset_turn_off_power_rails();

	/* clean-up internal variable */
	power_request = POWER_REQ_NONE;
}

/*****************************************************************************/

/**
 * Power off the AP
 */
static void power_off(void)
{
	/* Check the power off status */
	if (!is_system_powered())
		return;

	/* Call hooks before we drop power rails */
	hook_notify(HOOK_CHIPSET_SHUTDOWN);
	/* switch off all rails */
	chipset_turn_off_power_rails();

	lid_opened = 0;
	enable_sleep(SLEEP_MASK_AP_RUN);
	CPRINTS("power shutdown complete");
}

/**
 * Check if there has been a power-on event
 *
 * This checks all power-on event signals and returns non-zero if any have been
 * triggered (with debounce taken into account).
 *
 * @return non-zero if there has been a power-on event, 0 if not.
 */
static int check_for_power_on_event(void)
{
	int ap_off_flag;

	ap_off_flag = system_get_reset_flags() & RESET_FLAG_AP_OFF;
	system_clear_reset_flags(RESET_FLAG_AP_OFF);
	/* check if system is already ON */
	if (power_get_signals() & IN_POWER_GOOD) {
		if (ap_off_flag) {
			CPRINTS("system is on, but RESET_FLAG_AP_OFF is on");
			return POWER_ON_CANCEL;
		}
		CPRINTS("system is on, thus clear auto_power_on");
		/* no need to arrange another power on */
		auto_power_on = 0;
		return POWER_ON_BY_IN_POWER_GOOD;
	}
	if (ap_off_flag) {
		CPRINTS("RESET_FLAG_AP_OFF is on");
		power_off();
		return POWER_ON_CANCEL;
	}

	CPRINTS("POWER_GOOD is not asserted");

	/* power on requested at EC startup for recovery */
	if (auto_power_on) {
		auto_power_on = 0;
		return POWER_ON_BY_AUTO_POWER_ON;
	}

	/* Check lid open */
	if (lid_opened) {
		lid_opened = 0;
		return POWER_ON_BY_LID_OPEN;
	}

	/* check for power button press */
	if (power_button_is_pressed())
		return POWER_ON_BY_POWER_BUTTON_PRESSED;

	if (power_request == POWER_REQ_ON) {
		power_request = POWER_REQ_NONE;
		return POWER_ON_BY_POWER_REQ_NONE;
	}

	return POWER_OFF_CANCEL;
}

/**
 * Power on the AP
 */
static void power_on(void)
{
	/*
	 * When power_on() is called, we are at S5S3. Initialize components
	 * to ready state before AP is up.
	 */
	hook_notify(HOOK_CHIPSET_PRE_INIT);

	set_pmic_pwron(1);

	disable_sleep(SLEEP_MASK_AP_RUN);
	/* Call hooks now that AP is running */
	hook_notify(HOOK_CHIPSET_STARTUP);

	CPRINTS("AP running ...");
}

void chipset_reset(int is_cold)
{
	if (is_cold) {
		CPRINTS("EC triggered cold reboot");
		power_off();
		/* After POWER_GOOD is dropped off,
		 * the system will be on again
		 */
		power_request = POWER_REQ_ON;
	} else {
		CPRINTS("EC triggered warm reboot");
		/* TODO(waihong): Try another secured way to reset AP */
		set_pmic_pwron(1);
		usleep(PMIC_WARM_RESET_H_HOLD_TIME);
		set_pmic_pwron(0);
	}
}

enum power_state power_handle_state(enum power_state state)
{
	int value;
	static int boot_from_g3;

	switch (state) {
	case POWER_G3:
		boot_from_g3 = check_for_power_on_event();
		if (boot_from_g3)
			return POWER_G3S5;
		break;

	case POWER_G3S5:
		return POWER_S5;

	case POWER_S5:
		if (boot_from_g3) {
			value = boot_from_g3;
			boot_from_g3 = 0;
		} else {
			value = check_for_power_on_event();
		}

		if (value) {
			CPRINTS("power on %d", value);
			return POWER_S5S3;
		}
		return state;

	case POWER_S5S3:
		power_on();
		if (power_wait_signals(IN_POWER_GOOD) == EC_SUCCESS) {
			CPRINTS("POWER_GOOD seen");
			if (power_button_wait_for_release(
					DELAY_SHUTDOWN_ON_POWER_HOLD) ==
					EC_SUCCESS) {
				power_button_was_pressed = 0;
				set_pmic_pwron(0);

				/* Call hooks now that AP is running */
				hook_notify(HOOK_CHIPSET_STARTUP);

				return POWER_S3;
			}
			CPRINTS("long-press button, shutdown");
			power_off();
			/*
			 * Since the AP may be up already, return S0S3
			 * state to go through the suspend hook.
			 */
			return POWER_S0S3;
		}
		CPRINTS("POWER_GOOD not seen in time");

		chipset_turn_off_power_rails();
		return POWER_S5;

	case POWER_S3:
		if (!(power_get_signals() & IN_POWER_GOOD))
			return POWER_S3S5;
		return state;

	case POWER_S3S0:
		disable_sleep(SLEEP_MASK_AP_RUN);
		hook_notify(HOOK_CHIPSET_RESUME);
		return POWER_S0;

	case POWER_S0:
		value = check_for_power_off_event();
		if (value) {
			CPRINTS("power off %d", value);
			power_off();
			return POWER_S0S3;
		}
		return state;

	case POWER_S0S3:
		/*
		 * if the power button is pressing, we need cancel the long
		 * press timer, otherwise EC will crash.
		 */
		if (power_button_was_pressed)
			timer_cancel(TASK_ID_CHIPSET);

		/* Call hooks here since we don't know it prior to AP suspend */
		hook_notify(HOOK_CHIPSET_SUSPEND);
		enable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S3;

	case POWER_S3S5:
		power_button_wait_for_release(-1);
		power_button_was_pressed = 0;
		return POWER_S5;

	case POWER_S5G3:
		return POWER_G3;
	}

	return state;
}

static void powerbtn_sdm845_changed(void)
{
	task_wake(TASK_ID_CHIPSET);
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, powerbtn_sdm845_changed,
	     HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Console debug command */

static const char *power_req_name[POWER_REQ_COUNT] = {
	"none",
	"off",
	"on",
};

/* Power states that we can report */
enum power_state_t {
	PSTATE_UNKNOWN,
	PSTATE_OFF,
	PSTATE_ON,
	PSTATE_COUNT,
};

static const char * const state_name[] = {
	"unknown",
	"off",
	"on",
};

static int command_power(int argc, char **argv)
{
	int v;

	if (argc < 2) {
		enum power_state_t state;

		state = PSTATE_UNKNOWN;
		if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
			state = PSTATE_OFF;
		if (chipset_in_state(CHIPSET_STATE_ON))
			state = PSTATE_ON;
		ccprintf("%s\n", state_name[state]);

		return EC_SUCCESS;
	}

	if (!parse_bool(argv[1], &v))
		return EC_ERROR_PARAM1;

	power_request = v ? POWER_REQ_ON : POWER_REQ_OFF;
	ccprintf("Requesting power %s\n", power_req_name[power_request]);
	task_wake(TASK_ID_CHIPSET);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(power, command_power,
			"on/off",
			"Turn AP power on/off");
