/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Servo state machine.
 */
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "uart_bitbang.h"
#include "uartn.h"
#include "usb_i2c.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/*
 * Servo states.  These are a little more complex than the AP and EC state
 * machines because servo also has an UNDETECTABLE state where we're driving
 * the EC UART TX line ourselves and can't detect servo driving it.
 */
static enum {
	/*
	 * Servo known disconnected, because nobody is driving detect.
	 *
	 *	!detect_available -> UNDETECTABLE
	 *	gpio=1 -> connect() -> CONNECTED
	 */
	STATE_DISCONNECTED = 0,

	/*
	 * Servo is not knowable because we're driving detect.
	 *
	 *	gpio=0 -> DISCONNECTED
	 *	gpio=1 -> connect() -> CONNECTED
	 */
	STATE_UNDETECTABLE,

	/*
	 * Servo known connected, because servo is driving detect.
	 *
	 *	!detect_available -> disconnect() -> UNDETECTABLE
	 *	gpio=0 -> DEBOUNCING
	 */
	STATE_CONNECTED,

	/*
	 * Servo was connected, but we saw detect deasserted and are debouncing
	 * to see if it stays deasserted - at which point we'll decide that
	 * it's disconnected.
	 *
	 *	!detect_available -> disconnect() -> UNDETECTABLE
	 *	gpio=0 after 1 sec -> disconnect() -> DISCONNECTED
	 *	gpio=1 -> CONNECTED
	 */
	STATE_DEBOUNCING,
} state = STATE_DISCONNECTED;

int servo_is_connected(void)
{
	/*
	 * If we're connected, we definitely know we are.  If we're debouncing,
	 * then we were connected and might still be.  In either case, it's not
	 * safe to allow ports to be connected.
	 */
	return (state == STATE_CONNECTED || state == STATE_DEBOUNCING);
}

/**
 * Check if we can tell servo is connected.
 *
 * @return 1 if we can tell if servo is connected, 0 if we can't tell.
 */
static int servo_detectable(void)
{
	/*
	 * If we are driving the UART transmit line to the EC, then we can't
	 * check to see if servo is also doing so.
	 *
	 * We also need to check if we're bit-banging the EC UART, because in
	 * that case, the UART transmit line is directly controlled as a GPIO
	 * and can be high even if UART TX is disconnected.
	 */
	return !(uart_tx_is_connected(UART_EC) ||
		 uart_bitbang_is_enabled(UART_EC));
}

/**
 * Handle servo being disconnected
 */
static void servo_disconnect(void)
{
	CPRINTS("Servo disconnect");
	state = STATE_DISCONNECTED;
	rdd_update_state();
}

/**
 * Handle servo being connected
 */
static void servo_connect(void)
{
	CPRINTS("Servo connect");
	state = STATE_CONNECTED;
	rdd_update_state();
}

/**
 * Servo state machine
 */
static void check_servo(void)
{
	/* Disable interrupts if we had them on for debouncing */
	gpio_disable_interrupt(GPIO_DETECT_SERVO);

	/* If we're driving the UARTs, we can't detect servo */
	if (!servo_detectable()) {
		/* We're driving one port; might as well drive them all */
		if (servo_is_connected())
			servo_disconnect();

		state = STATE_UNDETECTABLE;
		return;
	}

	/* Handle detecting servo */
	if (gpio_get_level(GPIO_DETECT_SERVO)) {
		/*
		 * If we were debouncing, we hadn't yet disconnected so we can
		 * go straight back.
		 */
		if (state == STATE_DEBOUNCING)
			state = STATE_CONNECTED;

		/* If we're already connected, we're done */
		if (state == STATE_CONNECTED)
			return;

		/* Otherwise, we need to connect */
		servo_connect();
		return;
	}

	/*
	 * If servo has become detectable but wasn't detected above, assume
	 * it's disconnected.
	 */
	if (state == STATE_UNDETECTABLE)
		state = STATE_DISCONNECTED;

	/* Servo wasn't detected.  If we're already disconnected, done. */
	if (state == STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == STATE_DEBOUNCING) {
		servo_disconnect();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = STATE_DEBOUNCING;
	gpio_enable_interrupt(GPIO_DETECT_SERVO);
}
DECLARE_HOOK(HOOK_SECOND, check_servo, HOOK_PRIO_DEFAULT);

/**
 * Servo state initialization.  Called from board_init().
 */
void servo_init(void)
{
	/* The interrupt is enabled by default, but we don't need it */
	gpio_disable_interrupt(GPIO_DETECT_SERVO);

	/*
	 * Check to see if servo is connected now, before CCD can enable EC
	 * UART TX and block detection.  It's ok to call servo_connect()
	 * directly because we're already in the HOOK task.
	 *
	 * Note that if servo happens to be driving a 0-bit at this point we'll
	 * miss detecting it.  We could debounce in that case, but this simple
	 * check preserves how Cr50 worked previously.
	 */
	if (servo_detectable() && gpio_get_level(GPIO_DETECT_SERVO))
		servo_connect();
}

/**
 * Deferred handler for servo detect interrupt
 */
static void servo_reconnect(void)
{
	/* If we were debouncing, go back to connected */
	if (state == STATE_DEBOUNCING)
		state = STATE_CONNECTED;
}
DECLARE_DEFERRED(servo_reconnect);

/**
 * Interrupt handler for servo detect asserted
 */
void servo_detect_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_DETECT_SERVO);

	/*
	 * If we're still debouncing and this interrupt is because servo is
	 * actually detectable (vs. we're driving the detect pin now), queue a
	 * transition back to connected.
	 */
	if (state == STATE_DEBOUNCING && servo_detectable())
		hook_call_deferred(&servo_reconnect_data, 0);
}
