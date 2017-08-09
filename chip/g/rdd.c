/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "rdd.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_api.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

/*
 * The default PROG_DEBUG_STATE_MAP value. Used to tell the to controller send
 * an interrupt when CC1/2 are detected to be in the defined voltage range of a
 * debug accessory.
 */
#define DETECT_DEBUG 0x420

/*
 * The interrupt only triggers when the debug state is detected.  If we want to
 * trigger an interrupt when the debug state is *not* detected, we need to
 * program the bit-inverse.
 */
#define DETECT_DISCONNECT (~DETECT_DEBUG & 0xffff)

/* State of RDD CC detection */
static enum device_state state = DEVICE_STATE_DISCONNECTED;

/* Force detecting a debug accessory (ignore RDD CC detect hardware) */
static int force_detected;

/**
 * Get instantaneous cable detect state
 *
 * @return 1 if debug accessory is detected, 0 if not detected
 */
static int rdd_is_detected(void)
{
	uint8_t cc1 = GREAD_FIELD(RDD, INPUT_PIN_VALUES, CC1);
	uint8_t cc2 = GREAD_FIELD(RDD, INPUT_PIN_VALUES, CC2);

	return (cc1 == cc2 && (cc1 == 3 || cc1 == 1));
}

/**
 * Enable/disable interrupt for debug accessory connect
 *
 * @param enable	1 to enable, 0 to disable
 */
static void enable_interrupt(int enable)
{
	/* Clear any pending interrupts */
	GWRITE_FIELD(RDD, INT_STATE, INTR_DEBUG_STATE_DETECTED, 1);

	/* Enable/disable the interrupt */
	GWRITE_FIELD(RDD, INT_ENABLE, INTR_DEBUG_STATE_DETECTED,
		     enable ? 1 : 0);
}

/**
 * Handle debug accessory disconnecting
 */
static void rdd_disconnect(void)
{
	CPRINTS("Debug accessory disconnect");
	state = DEVICE_STATE_DISCONNECTED;

	/*
	 * Stop pulling CCD_MODE_L low.  The internal pullup configured in the
	 * pinmux will pull the signal back high, unless the EC is also pulling
	 * it low.
	 *
	 * This disables the SBUx muxes, if we were the only one driving
	 * CCD_MODE_L.
	 */
	gpio_set_flags(GPIO_CCD_MODE_L, GPIO_INPUT);
}

/**
 * Handle debug accessory connecting
 *
 * This can be deferred from both rdd_detect() and the interrupt handler, so
 * it needs to check the current state to determine whether we're already
 * connected.
 */
static void rdd_connect(void)
{
	/* If we were debouncing, we're done, and still connected */
	if (state == DEVICE_STATE_DEBOUNCING ||
	    state == DEVICE_STATE_DEBOUNCING2)
		state = DEVICE_STATE_CONNECTED;

	/* If we're already connected, done */
	if (state == DEVICE_STATE_CONNECTED)
		return;

	/* We were previously disconnected, so connect */
	CPRINTS("Debug accessory connect");
	state = DEVICE_STATE_CONNECTED;

	/* Start pulling CCD_MODE_L low to enable the SBUx muxes */
	GWRITE(PINMUX, DIOM1_SEL, GC_PINMUX_GPIO0_GPIO5_SEL);
	gpio_set_flags(GPIO_CCD_MODE_L, GPIO_OUT_LOW);
}
DECLARE_DEFERRED(rdd_connect);

/**
 * Debug accessory detect interrupt
 */
static void rdd_interrupt(void)
{
	delay_sleep_by(1 * SECOND);

	/* Call the deferred handler */
	hook_call_deferred(&rdd_connect_data, 0);

	/* Disable our interrupt; this also clears the pending interrupt */
	enable_interrupt(0);
}
DECLARE_IRQ(GC_IRQNUM_RDD0_INTR_DEBUG_STATE_DETECTED_INT, rdd_interrupt, 1);

/**
 * RDD CC detect state machine
 */
static void rdd_detect(void)
{
	/* Disable interrupt if we had it on for debouncing */
	enable_interrupt(0);

	/* Handle detecting device */
	if (force_detected || rdd_is_detected()) {
		rdd_connect();
		return;
	}

	/* CC wasn't detected.  If we're already disconnected, done. */
	if (state == DEVICE_STATE_DISCONNECTED)
		return;

	/* If this is our first second of debouncing, advance */
	if (state == DEVICE_STATE_DEBOUNCING) {
		state = DEVICE_STATE_DEBOUNCING2;
		return;
	}

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == DEVICE_STATE_DEBOUNCING2) {
		rdd_disconnect();
		return;
	}

	/*
	 * Otherwise, we were connected but the accessory seems to be
	 * disconnected right now.  But PD negotiation (e.g. during EC reset or
	 * sysjump) can alter the CC voltage, so we need to debounce this
	 * signal for a couple seconds to make sure it's consistent.
	 */
	state = DEVICE_STATE_DEBOUNCING;
	enable_interrupt(1);
}
/*
 * Bump up priority so this runs before the CCD_MODE_L state machine, because
 * we can change CCD_MODE_L.
 */
DECLARE_HOOK(HOOK_SECOND, rdd_detect, HOOK_PRIO_DEFAULT - 1);

void init_rdd_state(void)
{
	/* Enable RDD hardware */
	clock_enable_module(MODULE_RDD, 1);
	GWRITE(RDD, POWER_DOWN_B, 1);

	/* Configure to detect accessory connected */
	GWRITE(RDD, PROG_DEBUG_STATE_MAP, DETECT_DEBUG);

	/*
	 * Enable interrupt for detecting CC.  This minimizes the time before
	 * we transition to cable-detected at boot.
	 */
	task_enable_irq(GC_IRQNUM_RDD0_INTR_DEBUG_STATE_DETECTED_INT);
	enable_interrupt(1);
}

void force_rdd_detect(int enable)
{
	force_detected = enable;

	/*
	 * If we're forcing detection, trigger then connect handler early.
	 *
	 * Otherwise, we'll revert to the normal logic of checking the RDD
	 * hardware CC state.
	 */
	if (force_detected)
		hook_call_deferred(&rdd_connect_data, 0);
}

int rdd_detect_is_forced(void)
{
	return force_detected;
}
