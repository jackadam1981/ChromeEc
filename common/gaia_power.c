/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GAIA SoC power sequencing module for Chrome EC */

#include "board.h"
#include "chipset.h"  /* This module implements chipset functions too */
#include "console.h"
#include "gpio.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

/* Time necessary for the 5v regulator output to stabilize */
#define DELAY_5V_SETUP        1000  /* 1ms */

/* Delay between 1.35v and 3.3v rails startup */
#define DELAY_RAIL_STAGGERING 100  /* 100us */

/* Long power key press to force shutdown */
#define DELAY_FORCE_SHUTDOWN  8000000 /* 8s */

/*
 * If the power key is pressed to turn on, then held for this long, we
 * power off.
 */
#define DELAY_SHUTDOWN_ON_POWER_HOLD	(16 * 1000000)

/* Delay after power button release before we release GPIO_PMIC_PWRON_L */
#define DELAY_RELEASE_PWRON	1000000 /* 1s */

/* debounce time to prevent accidental power-on after keyboard power off */
#define KB_PWR_ON_DEBOUNCE    250    /* 250us */

/* PMIC fails to set the LDO2 output */
#define PMIC_TIMEOUT          100000  /* 100ms */

/* Default timeout for input transition */
#define FAIL_TIMEOUT          500000 /* 500ms */


/* Application processor power state */
static int ap_on;

/* simulated event state */
static int force_signal = -1;
static int force_value;

/* 1 if the power button was pressed last time we checked */
static char power_button_was_pressed;

/* time where we will power off, if power button still held down */
static timestamp_t power_off_deadline;

/* 1 if we have released GPIO_PMIC_PWRON_L */
static char pwron_released;

/* time where we will release GPIO_PMIC_PWRON_L */
static timestamp_t pwron_deadline;

/*
 * Wait for GPIO "signal" to reach level "value".
 * Returns EC_ERROR_TIMEOUT if timeout before reaching the desired state.
 *
 * @param signal	Signal to watch
 * @param value		Value to watch for
 * @param timeout	Timeout in microseconds from now, or -1 to wait forever
 * @return 0 if signal did change to required value, EC_ERROR_TIMEOUT if we
 * timed out first.
 */
static int wait_in_signal(enum gpio_signal signal, int value, int timeout)
{
	timestamp_t deadline;
	timestamp_t now = get_time();

	deadline.val = now.val + timeout;

	while (((force_signal != signal) || (force_value != value)) &&
			gpio_get_level(signal) != value) {
		now = get_time();
		if (timeout < 0) {
			task_wait_event(-1);
		} else if (timestamp_expired(deadline) ||
				(task_wait_event(deadline.val - now.val) ==
					TASK_EVENT_TIMER)) {
			CPRINTF("Timeout waiting for GPIO %d/%s\n", signal,
				    gpio_get_name(signal));
			return EC_ERROR_TIMEOUT;
		}
	}

	return EC_SUCCESS;
}

/*
 * Check for some event triggering the shutdown.
 *
 * It can be either a long power button press or a shutdown triggered from the
 * AP and detected by reading XPSHOLD.
 *
 * @return 1 if a shutdown should happen, 0 if not
 */
static int check_for_power_off_event(void)
{
	timestamp_t now;
	int pressed = 0;

	/* Check for power button press */
	if (gpio_get_level(GPIO_KB_PWR_ON_L) == 0) {
		udelay(KB_PWR_ON_DEBOUNCE);
		if (gpio_get_level(GPIO_KB_PWR_ON_L) == 0)
			pressed = 1;
	}

	now = get_time();
	if (pressed) {
		if (!power_button_was_pressed) {
			power_off_deadline.val = now.val + DELAY_FORCE_SHUTDOWN;
			CPRINTF("Waiting for long power press %u\n",
				power_off_deadline.le.lo);
			timer_arm(power_off_deadline, TASK_ID_GAIAPOWER);
		} else if (timestamp_expired(power_off_deadline)) {
			CPRINTF("Power off after long power press now=%u, %u\n",
				now.le.lo, power_off_deadline.le.lo);
			return 1;
		}
	} else if (power_button_was_pressed) {
		CPRINTF("Cancel power off\n");
	}
	power_button_was_pressed = pressed;

	/* XPSHOLD released by AP : shutdown immediatly */
	if (gpio_get_level(GPIO_SOC1V8_XPSHOLD) == 0)
		return 1;

	return 0;
}

void gaia_power_event(enum gpio_signal signal)
{
	/* Wake up the task */
	task_wake(TASK_ID_GAIAPOWER);
}

int gaia_power_init(void)
{
	/* Enable interrupts for our GPIOs */
	gpio_enable_interrupt(GPIO_KB_PWR_ON_L);
	gpio_enable_interrupt(GPIO_PP1800_LDO2);
	gpio_enable_interrupt(GPIO_SOC1V8_XPSHOLD);

	return EC_SUCCESS;
}


/*****************************************************************************/
/* Chipset interface */

/* Returns non-zero if the chipset is in the specified state. */
int chipset_in_state(enum chipset_state in_state)
{
	switch (in_state) {
	case CHIPSET_STATE_SOFT_OFF:
		return ap_on == 0;
	case CHIPSET_STATE_SUSPEND:
		/* TODO: implement */
		return 0;
	case CHIPSET_STATE_ON:
		return ap_on;
	}

	/* Should never get here since we list all states above, but compiler
	 * doesn't seem to understand that. */
	return 0;
}


/**
 * Check if there has been a power-on event
 *
 * This waits for the power button to be pressed, then returns whether it
 * is still pressed, after a debounce period
 *
 * @return 1 if there has been a power-on event, 0 if not
 */
static int check_for_power_on_event(void)
{
	/* wait for Power button press */
	wait_in_signal(GPIO_KB_PWR_ON_L, 0, -1);

	udelay(KB_PWR_ON_DEBOUNCE);
	return gpio_get_level(GPIO_KB_PWR_ON_L) == 0;
}

/**
 * Power on the AP
 *
 * @return 0 if ok, -1 on error (PP1800_LDO2 failed to come on)
 */
static int power_on(void)
{
	/* Enable 5v power rail */
	gpio_set_level(GPIO_EN_PP5000, 1);
	/* wait to have stable power */
	usleep(DELAY_5V_SETUP);

	/* Startup PMIC */
	gpio_set_level(GPIO_PMIC_PWRON_L, 0);
	/* wait for all PMIC regulators to be ready */
	wait_in_signal(GPIO_PP1800_LDO2, 1, PMIC_TIMEOUT);

	/* if PP1800_LDO2 did not come up (e.g. PMIC_TIMEOUT was
		* reached), turn off 5v rail and start over */
	if (gpio_get_level(GPIO_PP1800_LDO2) == 0) {
		gpio_set_level(GPIO_EN_PP5000, 0);
		usleep(DELAY_5V_SETUP);
		CPUTS("Fatal error: PMIC failed to enable\n");
		return -1;
	}

	/* Enable DDR 1.35v power rail */
	gpio_set_level(GPIO_EN_PP1350, 1);
	/* wait to avoid large inrush current */
	usleep(DELAY_RAIL_STAGGERING);
	/* Enable 3.3v power rail */
	gpio_set_level(GPIO_EN_PP3300, 1);
	CPUTS("AP running ...\n");
	ap_on = 1;
	return 0;
}

/**
 * Wait for the power button to be released
 *
 * @return 0 if ok, -1 if power button failed to release
 */
static int wait_for_power_button_release(unsigned int timeout_us)
{
	/* wait for Power button release */
	wait_in_signal(GPIO_KB_PWR_ON_L, 1, timeout_us);

	udelay(KB_PWR_ON_DEBOUNCE);
	if (gpio_get_level(GPIO_KB_PWR_ON_L) == 0)
		return -1;
	return 0;
}

/**
 * Power off the AP
 */
static void power_off(void)
{
	/* switch off all rails */
	gpio_set_level(GPIO_EN_PP3300, 0);
	gpio_set_level(GPIO_EN_PP1350, 0);
	gpio_set_level(GPIO_PMIC_PWRON_L, 1);
	gpio_set_level(GPIO_EN_PP5000, 0);
	CPUTS("Shutdown complete.\n");
	ap_on = 0;
}

/**
 * Set a timer to release GPIO_PMIC_PWRON_L in the future
 */
static void set_pwron_timer(void)
{
	pwron_deadline = get_time();
	pwron_deadline.val += DELAY_RELEASE_PWRON;
	CPRINTF("Setting pwron timer\n");
	timer_arm(pwron_deadline, TASK_ID_GAIAPOWER);
	pwron_released = 0;
}

static void check_pwron_release(void)
{
	if (!pwron_released && timestamp_expired(pwron_deadline)) {
		pwron_released = 1;
		gpio_set_level(GPIO_PMIC_PWRON_L, 1);
		CPRINTF("Releasing pwron\n");
	}
}

/*****************************************************************************/

void gaia_power_task(void)
{
	gaia_power_init();
	ap_on = 0;

	while (1) {
		/* Wait until we need to power on, then power on */
		/* TODO(sjg@chromium.org) Go to deep sleep here */
		while (!check_for_power_on_event())
			task_wait_event(-1);

		/*
		 * If we can power on, and the power button is released,
		 * start running!
		 */
		if (!power_on() && !wait_for_power_button_release(
					DELAY_SHUTDOWN_ON_POWER_HOLD)) {
			/* Wait until we need to power off, then power off */
			power_button_was_pressed = 0;
			set_pwron_timer();
			while (!check_for_power_off_event()) {
				check_pwron_release();
				task_wait_event(-1);
			}
		}
		power_off();
		wait_for_power_button_release(-1);
	}
}

/*****************************************************************************/
/* Console debug command */

static int command_force_power(int argc, char **argv)
{
	/* simulate power button pressed */
	force_signal = GPIO_KB_PWR_ON_L;
	force_value = 1;
	/* Wake up the task */
	task_wake(TASK_ID_GAIAPOWER);
	/* wait 100 ms */
	usleep(100000);
	/* release power button */
	force_signal = -1;
	force_value = 0;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(forcepower, command_force_power);
