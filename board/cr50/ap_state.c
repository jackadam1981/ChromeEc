/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AP state machine
 */
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

static enum device_state state = DEVICE_STATE_DISCONNECTED;

int ap_is_on(void)
{
	/* Debouncing and connected are both still connected */
	return state != DEVICE_STATE_DISCONNECTED;
}

/**
 * Handle AP disconnect / suspend
 */
static void ap_disconnect(void)
{
	CPRINTS("AP disconnect");
	state = DEVICE_STATE_DISCONNECTED;

	/*
	 * If TPM is configured then the INT_AP_L signal is used as a low pulse
	 * trigger to sync transactions with the host. By default Cr50 is
	 * driving this line high, but when the AP powers off, the 1.8V rail
	 * that it's pulled up to will be off and cause excessive power to be
	 * consumed by the Cr50. Set INT_AP_L as an input while the AP is
	 * powered off.
	 */
	gpio_set_flags(GPIO_INT_AP_L, GPIO_INPUT);

	rdd_update_state();

	/*
	 * We don't enable deep sleep on ARM devices yet, as its processing
	 * there will require more support on the AP side than is available
	 * now.
	 *
	 * Note: Presence of platform reset is a poor indicator of deep sleep
	 * support.  It happens to be correlated with ARM vs x86 at present.
	 */
	if (board_use_plt_rst())
		enable_deep_sleep();
}

/**
 * Handle AP connect / resume
 */
static void ap_connect(void)
{
	CPRINTS("AP connect");
	state = DEVICE_STATE_CONNECTED;

	/*
	 * AP is powering up, set the host sync signal to output and set it
	 * high which is the default level.
	 */
	gpio_set_flags(GPIO_INT_AP_L, GPIO_OUT_HIGH);
	gpio_set_level(GPIO_INT_AP_L, 1);

	rdd_update_state();

	if (board_use_plt_rst())
		disable_deep_sleep();
}

/**
 * Handle AP connect from a deferred interrupt handler.
 *
 * Needs to make additional state checks to avoid double-connect in case
 * ap_detect() has run in the meantime.
 */
void ap_connect_deferred(void)
{
	/* If we were debouncing, go back to connected */
	if (state == DEVICE_STATE_DEBOUNCING)
		state = DEVICE_STATE_CONNECTED;

	/* If we're not connected, now would be good */
	if (state != DEVICE_STATE_CONNECTED)
		ap_connect();
}
DECLARE_DEFERRED(ap_connect_deferred);

/**
 * Interrupt handler for AP detect asserted
 */
void ap_detect_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_DETECT_AP);
	hook_call_deferred(&ap_connect_deferred_data, 0);
}

void init_ap_state(void)
{
	/*
	 * Enable interrupt for detecting the AP.  This minimizes the time
	 * before we transition the AP to connected at boot.
	 */
	if (!board_use_plt_rst())
		gpio_enable_interrupt(GPIO_DETECT_AP);
}

/**
 * Detect state machine
 */
static void ap_detect(void)
{
	int detect;

	if (board_use_plt_rst()) {
		/* AP is detected if platform reset is deasserted */
		detect = gpio_get_level(GPIO_TPM_RST_L);
	} else {
		/* Disable interrupts if we had them on for debouncing */
		gpio_disable_interrupt(GPIO_DETECT_AP);

		/* AP is detected if it's driving its UART TX signal */
		detect = gpio_get_level(GPIO_DETECT_AP);
	}

	/* Handle detecting device */
	if (detect) {
		/* If we were debouncing, go back to connected */
		if (state == DEVICE_STATE_DEBOUNCING)
			state = DEVICE_STATE_CONNECTED;

		/* If we're already connected, done */
		if (state == DEVICE_STATE_CONNECTED)
			return;

		if (board_use_plt_rst()) {
			/*
			 * The platform reset handler has not run yet;
			 * otherwise, it would have already connected the AP
			 * and we wouldn't get here.
			 *
			 * This can happen if the hook task calls ap_detect()
			 * before deferred_tpm_rst_isr().  In this case, the
			 * deferred handler is already pending so calling the
			 * ISR has no effect.
			 *
			 * But we may actually have missed the edge.  In that
			 * case, calling the ISR makes sure we don't miss the
			 * reset.  It will call ap_connect_from_tpm_rst() to
			 * do the connection.
			 */
			CPRINTS("AP connect calling tpm_rst_deasserted()");
			tpm_rst_deasserted(GPIO_TPM_RST_L);
		} else {
			/* We're responsible for connecting the AP */
			ap_connect();
		}

		return;
	}

	/* AP wasn't detected.  If we're already disconnected, done. */
	if (state == DEVICE_STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == DEVICE_STATE_DEBOUNCING) {
		ap_disconnect();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = DEVICE_STATE_DEBOUNCING;

	/* If we're using AP UART RX for detect, enable its interrupt */
	if (!board_use_plt_rst())
		gpio_enable_interrupt(GPIO_DETECT_AP);
}
DECLARE_HOOK(HOOK_SECOND, ap_detect, HOOK_PRIO_DEFAULT);
