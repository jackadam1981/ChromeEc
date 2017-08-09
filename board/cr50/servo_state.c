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

static enum device_state state = DEVICE_STATE_DISCONNECTED;

int servo_is_connected(void)
{
	/*
	 * If we're connected, we definitely know we are.  If we're debouncing,
	 * then we were connected and might still be.  In either case, it's not
	 * safe to allow ports to be connected.
	 */
	return (state == DEVICE_STATE_CONNECTED ||
		state == DEVICE_STATE_DEBOUNCING);
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
	state = DEVICE_STATE_DISCONNECTED;
	rdd_update_state();
}

/**
 * Handle servo being connected
 */
static void servo_connect(void)
{
	CPRINTS("Servo connect");
	state = DEVICE_STATE_CONNECTED;
	rdd_update_state();
}

/**
 * Servo state machine
 */
static void servo_detect(void)
{
	/* Disable interrupts if we had them on for debouncing */
	gpio_disable_interrupt(GPIO_DETECT_SERVO);

	/* If we're driving the UARTs, we can't detect servo */
	if (!servo_detectable()) {
		/* We're driving one port; might as well drive them all */
		if (servo_is_connected())
			servo_disconnect();

		state = DEVICE_STATE_UNDETECTABLE;
		return;
	}

	/* Handle detecting servo */
	if (gpio_get_level(GPIO_DETECT_SERVO)) {
		/*
		 * If we were debouncing, we hadn't yet disconnected so we can
		 * go straight back.
		 */
		if (state == DEVICE_STATE_DEBOUNCING)
			state = DEVICE_STATE_CONNECTED;

		/* If we're not connected, connect */
		if (state != DEVICE_STATE_CONNECTED)
			servo_connect();
		return;
	}

	/*
	 * If servo has become detectable but wasn't detected above, assume
	 * it's disconnected.
	 */
	if (state == DEVICE_STATE_UNDETECTABLE)
		state = DEVICE_STATE_DISCONNECTED;

	/* Servo wasn't detected.  If we're already disconnected, done. */
	if (state == DEVICE_STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == DEVICE_STATE_DEBOUNCING) {
		servo_disconnect();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = DEVICE_STATE_DEBOUNCING;
	gpio_enable_interrupt(GPIO_DETECT_SERVO);
}
/*
 * Do this at slightly elevated priority so it runs before rdd_check_pin() and
 * ec_detect().  This increases the odds that we'll detect servo before
 * detecting the EC.  If ec_detect() ran first, it could turn on TX to the EC
 * UART before we had a chance to detect servo.  This is still a little bit of
 * a race condition.
 */
DECLARE_HOOK(HOOK_SECOND, servo_detect, HOOK_PRIO_DEFAULT - 1);

/**
 * Servo state initialization.  Called from board_init().
 */
void init_servo_state(void)
{
	/*
	 * Check to see if servo is connected now, before CCD can enable EC
	 * UART TX and block detection.  It's ok to call servo_connect()
	 * directly because we're already in the HOOK task.
	 *
	 * Note that if servo happens to be driving a 0-bit at this point we'll
	 * miss detecting it.  We could debounce in that case, but this simple
	 * check preserves how Cr50 worked previously.
	 */
	// Should just start debouncing
	if (servo_detectable() && gpio_get_level(GPIO_DETECT_SERVO))
		servo_connect();
}

/**
 * Deferred handler for servo detect interrupt
 */
static void servo_reconnect(void)
{
	/* If we were debouncing, go back to connected */
	if (state == DEVICE_STATE_DEBOUNCING)
		state = DEVICE_STATE_CONNECTED;
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
	if (state == DEVICE_STATE_DEBOUNCING && servo_detectable())
		hook_call_deferred(&servo_reconnect_data, 0);
}
