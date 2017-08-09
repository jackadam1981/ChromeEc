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

static enum device_state state = DEVICE_STATE_DISCONNECTED;

int ec_is_on(void)
{
	/* Debouncing and connected are both still connected */
	return state != DEVICE_STATE_DISCONNECTED;
}

/**
 * Connect to the EC
 *
 * This can be deferred from both ec_detect() and the interrupt handler, so it
 * needs to check the current state to determine whether we're already
 * connected.
 */
static void ec_connect(void)
{
	/* If we were debouncing, we're done, and still connected */
	if (state == DEVICE_STATE_DEBOUNCING)
		state = DEVICE_STATE_CONNECTED;

	/* If we're already connected, done */
	if (state == DEVICE_STATE_CONNECTED)
		return;

	/* We were previously disconnected */
	CPRINTS("EC connect");
	state = DEVICE_STATE_CONNECTED;
	rdd_update_state();
}
DECLARE_DEFERRED(ec_connect);

/**
 * Interrupt handler for EC detect asserted.
 */
void ec_detect_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_DETECT_EC);
	hook_call_deferred(&ec_connect_data, 0);
}

void init_ec_state(void)
{
	/*
	 * Enable interrupt for detecting the EC.  This minimizes the time
	 * before we transition the EC to connected at boot.
	 */
	// But note that we want to block EC TX until after we finish
	// debouncing servo
	gpio_enable_interrupt(GPIO_DETECT_EC);
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
		ec_connect();
		return;
	}

	/* EC wasn't detected.  If we're already disconnected, done. */
	if (state == DEVICE_STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == DEVICE_STATE_DEBOUNCING) {
		CPRINTS("EC disconnect");
		state = DEVICE_STATE_DISCONNECTED;
		rdd_update_state();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = DEVICE_STATE_DEBOUNCING;
	gpio_enable_interrupt(GPIO_DETECT_EC);
}
DECLARE_HOOK(HOOK_SECOND, ec_detect, HOOK_PRIO_DEFAULT);
