/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC detect state machine.
 */
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

static enum {
	/*
	 * Disconnected, because EC is not driving detect.
	 *
	 *	gpio=1 -> connect() -> CONNECTED
	 */
	STATE_DISCONNECTED = 0,

	/*
	 * Connected, because EC is driving detect.
	 *
	 *	gpio=0 -> DEBOUNCING
	 */
	STATE_CONNECTED,

	/*
	 * Was connected, but we saw detect deasserted and are debouncing
	 * to see if it stays deasserted - at which point we'll decide that
	 * it's disconnected.
	 *
	 *	gpio=0 after 1 sec -> disconnect() -> DISCONNECTED
	 *	gpio=1 -> CONNECTED
	 */
	STATE_DEBOUNCING,
} state = STATE_DISCONNECTED;

int ec_is_connected(void)
{
	/* Debouncing and connected are both still connected */
	return state != STATE_DISCONNECTED;
}

/**
 * Detect state machine
 */
static void ec_detect(void)
{
	/* Disable interrupts if we had them on for debouncing */
	gpio_disable_interrupt(GPIO_DETECT_EC);

	/* Handle detecting device */
	if (gpio_get_level(GPIO_DETECT_EC)) {
		/* If we were debouncing, go back to connected */
		if (state == STATE_DEBOUNCING)
			state = STATE_CONNECTED;

		/* If we're already connected, done */
		if (state == STATE_CONNECTED)
			return;

		/* We were previously disconnected */
		CPRINTS("EC connect");
		state = STATE_CONNECTED;
		rdd_update_state();
		return;
	}

	/* EC wasn't detected.  If we're already disconnected, done. */
	if (state == STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == STATE_DEBOUNCING) {
		CPRINTS("EC disconnect");
		state = STATE_DISCONNECTED;
		rdd_update_state();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = STATE_DEBOUNCING;
	gpio_enable_interrupt(GPIO_DETECT_EC);
}
DECLARE_HOOK(HOOK_SECOND, ec_detect, HOOK_PRIO_DEFAULT);

/**
 * Deferred handler for AP detect interrupt so all state transitions are in the
 * hook task.
 */
static void ec_reconnect(void)
{
	/* If we were debouncing, go back to connected */
	if (state == STATE_DEBOUNCING)
		state = STATE_CONNECTED;
}
DECLARE_DEFERRED(ec_reconnect);

/**
 * Interrupt handler for EC detect asserted.
 */
void ec_detect_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_DETECT_EC);
	hook_call_deferred(&ec_reconnect_data, 0);
}
