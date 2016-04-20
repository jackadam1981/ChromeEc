/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Lid switch module for Chrome EC */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "lid_switch.h"
#include "timer.h"
#include "util.h"
#include "chipset.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SWITCH, outstr)
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ## args)

#define LID_DEBOUNCE_US    (30 * MSEC)  /* Debounce time for lid switch */
#define LID_SWITCH_US    (200 * MSEC)

/* if no X-macro is defined for LID switch GPIO, use GPIO_LID_OPEN as default */
#ifndef CONFIG_LID_SWITCH_GPIO_LIST
#define CONFIG_LID_SWITCH_GPIO_LIST LID_GPIO(GPIO_LID_OPEN)
#endif

static int debounced_lid_open;		/* Debounced lid state */
static int forced_lid_open;	/* Forced lid open */

static int processing = 0; /* set while an event is being processed */
static int pending = 0;    /* set when an event is deferred */

static int count = 0;      /* number of retries */

/**
 * Get raw lid switch state.
 *
 * @return 1 if lid is open, 0 if closed.
 */
static int raw_lid_open(void)
{
#define LID_GPIO(gpio) || gpio_get_level(gpio)
	return (forced_lid_open CONFIG_LID_SWITCH_GPIO_LIST) ? 1 : 0;
#undef LID_GPIO
}

/*
 * Initiate a host event
 * Invoke the lid change hook
 * Setup a deferred call which will check for
 * the desired end state
 */
void lid_change_with_end_state_check(int event)
{
	hook_notify(HOOK_LID_CHANGE);
	host_set_single_event(event);

	count = 0;
	hook_call_deferred(end_state_check, LID_SWITCH_US);
}

/*
 * Invoked after a desired state is reached
 * or we give up after a fixed number of tries
 */
void send_pending_event(void)
{
	if (pending) {
		CPRINTS("Sending pending lid event");

		processing = pending;
		lid_change_with_end_state_check(pending);
	}
}

/*
 * Check for the desired end state based on the
 * current event being processed
 */
int in_end_state(void)
{
	if ((processing == EC_HOST_EVENT_LID_OPEN) && chipset_in_state(CHIPSET_STATE_ON))
		return 1;
	else if (((processing == EC_HOST_EVENT_LID_CLOSED) &&
		(chipset_in_state(CHIPSET_STATE_SUSPEND) ||
		chipset_in_state(CHIPSET_STATE_STANDBY) ||
		chipset_in_state(CHIPSET_STATE_SOFT_OFF))))
		return 1;
	else
		return 0;
}

/*
 * Checks if the desired end state has been reached.
 * If the state has been reached, then any pending events
 * are processed and processing/pending bits cleared
 *
 * If not reached, a new deferred call is setup if count < max_retries
 *
 * If max number of retries have been reached, then we give up and
 * clear any pending events without processing.
 */
void end_state_check(void)
{
	if (in_end_state()) {
		send_pending_event();
		processing = pending = 0;

		return;
	}

	if (count++ < 10)
		hook_call_deferred(end_state_check, LID_SWITCH_US);
	else
		processing = pending = 0;

}
DECLARE_DEFERRED(end_state_check);

/*
 * check if any event is being processed
 * if not, process it immediately
 * if yes, mark the event as pending for deferred processing
 */
void handle_lid_change_event(int event)
{
	if (!processing) {
		processing = event;
		lid_change_with_end_state_check(event);
	} else
		pending = event;
}

/**
 * Handle lid open.
 */
static void lid_switch_open(void)
{
	if (debounced_lid_open) {
		CPRINTS("lid already open");
		return;
	}

	CPRINTS("lid open");
	debounced_lid_open = 1;

	handle_lid_change_event(EC_HOST_EVENT_LID_OPEN);
}

/**
 * Handle lid close.
 */
static void lid_switch_close(void)
{
	if (!debounced_lid_open) {
		CPRINTS("lid already closed");
		return;
	}

	CPRINTS("lid close");
	debounced_lid_open = 0;

	handle_lid_change_event(EC_HOST_EVENT_LID_CLOSED);
}

test_mockable int lid_is_open(void)
{
	return debounced_lid_open;
}

/**
 * Lid switch initialization code
 */
static void lid_init(void)
{
	if (raw_lid_open())
		debounced_lid_open = 1;

	/* Enable interrupts, now that we've initialized */
#define LID_GPIO(gpio) gpio_enable_interrupt(gpio);
	CONFIG_LID_SWITCH_GPIO_LIST
#undef LID_GPIO
}
DECLARE_HOOK(HOOK_INIT, lid_init, HOOK_PRIO_INIT_LID);

/**
 * Handle debounced lid switch changing state.
 */
static void lid_change_deferred(void)
{
	const int new_open = raw_lid_open();

	/* If lid hasn't changed state, nothing to do */
	if (new_open == debounced_lid_open)
		return;

	if (new_open)
		lid_switch_open();
	else
		lid_switch_close();
}
DECLARE_DEFERRED(lid_change_deferred);

void lid_interrupt(enum gpio_signal signal)
{
	/* Reset lid debounce time */
	hook_call_deferred(lid_change_deferred, LID_DEBOUNCE_US);
}

static int command_lidopen(int argc, char **argv)
{
	lid_switch_open();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(lidopen, command_lidopen,
			NULL,
			"Simulate lid open",
			NULL);

static int command_lidclose(int argc, char **argv)
{
	lid_switch_close();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(lidclose, command_lidclose,
			NULL,
			"Simulate lid close",
			NULL);

/**
 * Host command to enable/disable lid opened.
 */
static int hc_force_lid_open(struct host_cmd_handler_args *args)
{
	const struct ec_params_force_lid_open *p = args->params;

	/* Override lid open if necessary */
	forced_lid_open = p->enabled ? 1 : 0;

	/* Make this take effect immediately; no debounce time */
	hook_call_deferred(lid_change_deferred, 0);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FORCE_LID_OPEN, hc_force_lid_open,
		     EC_VER_MASK(0));
