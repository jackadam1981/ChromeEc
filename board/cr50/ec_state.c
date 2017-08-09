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
#include "uart_bitbang.h"
#include "uartn.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

static enum device_state state = DEVICE_STATE_INIT;

int ec_is_on(void)
{
	/* Debouncing and on are both still on */
	return (state == DEVICE_STATE_DEBOUNCING || state == DEVICE_STATE_ON);
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
	if (state == DEVICE_STATE_INIT ||
	    state == DEVICE_STATE_INIT_DEBOUNCING) {
		/*
		 * Enable the UART peripheral so we start receiving on EC RX,
		 * but do not call uartn_tx_connect() to connect EC TX yet.  We
		 * need to be able to use EC TX to detect servo, so if we drive
		 * it right away that blocks us from detecting servo.
		 */
		// TODO: can simplify this state machine now, and put the
		// logic in rdd_update_state?
		CPRINTS("EC RX only");
		rdd_update_state();
		state = DEVICE_STATE_INIT_RX_ONLY;
		return;
	}

	/* If we were debouncing, we're done, and still on */
	if (state == DEVICE_STATE_DEBOUNCING)
		state = DEVICE_STATE_ON;

	/* If we're already on, done */
	if (state == DEVICE_STATE_ON)
		return;

	/* We were previously off */
	CPRINTS("EC on");
	state = DEVICE_STATE_ON;

	/* Update RDD state */
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

/**
 * Detect state machine
 */
static void ec_detect(void)
{
	/* Disable interrupts if we had them on for debouncing */
	gpio_disable_interrupt(GPIO_DETECT_EC);

	/* Handle detecting device */
	if (gpio_get_level(GPIO_DETECT_EC)) {
		/* Go back to connected */
		ec_connect();
		return;
	}

	/* EC wasn't detected.  If we're already off, done. */
	if (state == DEVICE_STATE_OFF)
		return;

	/* If we were debouncing, we're now sure we're off */
	if (state == DEVICE_STATE_DEBOUNCING ||
	    state == DEVICE_STATE_INIT_DEBOUNCING) {
		CPRINTS("EC off");
		state = DEVICE_STATE_OFF;
		rdd_update_state();
		return;
	}

	/* Otherwise, we were on and need to start debouncing */
	if (state == DEVICE_STATE_INIT)
		state = DEVICE_STATE_INIT_DEBOUNCING;
	else
		state = DEVICE_STATE_DEBOUNCING;
	gpio_enable_interrupt(GPIO_DETECT_EC);
}
DECLARE_HOOK(HOOK_SECOND, ec_detect, HOOK_PRIO_DEFAULT);
