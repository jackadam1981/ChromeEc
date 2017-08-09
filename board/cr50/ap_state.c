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
#include "uart_bitbang.h"
#include "uartn.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

static enum {
	/*
	 * Disconnected, because detect isn't driven.
	 *
	 *	gpio=1 -> connect() -> CONNECTED
	 *
	 * On boards with platform reset, connect may also be triggered by
	 * TPM reset deasserting, instead of the AP driving its UART.
	 */
	STATE_DISCONNECTED = 0,

	/*
	 * Connected, because AP is driving detect.
	 *
	 *	gpio=0 -> DEBOUNCING
	 *
	 * On boards with platform reset, the debouncing transition is
	 * triggered by TPM reset asserting.  On other boards, it's triggered
	 * by AP no longer driving its UART.
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

int ap_is_connected(void)
{
	/* Debouncing and connected are both still connected */
	return state != STATE_DISCONNECTED;
}

/**
 * Handle AP disconnect / suspend
 */
static void ap_disconnect(void)
{
	CPRINTS("AP disconnect");
	state = STATE_DISCONNECTED;

	/*
	 * If I2C TPM is configured then the INT_AP_L signal is used as
	 * a low pulse trigger to sync I2C transactions with the
	 * host. By default Cr50 is driving this line high, but when the
	 * AP powers off, the 1.8V rail that it's pulled up to will be
	 * off and cause exessive power to be consumed by the Cr50. Set
	 * INT_AP_L as an input while the AP is powered off.
	 */
	gpio_set_flags(GPIO_INT_AP_L, GPIO_INPUT);

	disable_uart(UART_AP);

	/*
	 * We don't enable deep sleep on ARM devices yet, as its processing
	 * there will require more support on the AP side than is available
	 * now.
	 */
	if (board_use_plt_rst())
		enable_deep_sleep();

	/*
	 * Trigger the shutdown hook.
	 *
	 * The only consumer of this is common/system.c, which uses it for
	 * reboot-after-shutdown.  If we don't use that in Cr50, we can get
	 * rid of this call.
	 */
	hook_notify(HOOK_CHIPSET_SHUTDOWN);
}

/**
 * Handle AP connect / resume
 */
static void ap_connect(void)
{
	CPRINTS("AP connect");
	state = STATE_CONNECTED;

	/*
	 * AP is powering up, set the I2C host sync signal to output and set
	 * it high which is the default level.
	 */
	gpio_set_flags(GPIO_INT_AP_L, GPIO_OUT_HIGH);
	gpio_set_level(GPIO_INT_AP_L, 1);

	enable_uart(UART_AP);

	disable_deep_sleep();
}

/**
 * Handle AP connect from the deferred TPM reset interrupt handler
 */
void ap_connect_from_tpm_rst(void)
{
	/* If we were debouncing, go back to connected */
	if (state == STATE_DEBOUNCING)
		state = STATE_CONNECTED;

	/* If we're not connected, now would be good */
	if (state != STATE_CONNECTED)
		ap_connect();
}

/**
 * Detect state machine
 */
static void ap_detect(void)
{
	int detect;

	if (board_use_plt_rst()) {
		/* Use plt_rst_l for device detect purposes */
		detect = gpio_get_level(GPIO_TPM_RST_L);
	} else {
		/* Disable interrupts if we had them on for debouncing */
		gpio_disable_interrupt(GPIO_DETECT_AP);
		detect = gpio_get_level(GPIO_DETECT_AP);
	}

	/* Handle detecting device */
	if (detect) {
		/* If we were debouncing, go back to connected */
		if (state == STATE_DEBOUNCING)
			state = STATE_CONNECTED;

		/* If we're already connected, done */
		if (state == STATE_CONNECTED)
			return;

		if (board_use_plt_rst()) {
			/*
			 * The tpm reset handler has not run yet; otherwise, it
			 * would have already connected the AP and we wouldn't
			 * get here.
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
	if (state == STATE_DISCONNECTED)
		return;

	/* If we were debouncing, we're now sure we're disconnected */
	if (state == STATE_DEBOUNCING) {
		ap_disconnect();
		return;
	}

	/* Otherwise, we were connected and need to start debouncing */
	state = STATE_DEBOUNCING;

	/* If we're using AP UART RX for detect, enable its interrupt */
	if (!board_use_plt_rst())
		gpio_enable_interrupt(GPIO_DETECT_AP);
}
DECLARE_HOOK(HOOK_SECOND, ap_detect, HOOK_PRIO_DEFAULT);

/**
 * Deferred handler for AP detect interrupt so all state transitions are in the
 * hook task.
 */
static void ap_reconnect(void)
{
	/* If we were debouncing, go back to connected */
	if (state == STATE_DEBOUNCING)
		state = STATE_CONNECTED;
}
DECLARE_DEFERRED(ap_reconnect);

/**
 * Interrupt handler for AP detect asserted
 */
void ap_detect_asserted(enum gpio_signal signal)
{
	gpio_disable_interrupt(GPIO_DETECT_AP);
	hook_call_deferred(&ap_reconnect_data, 0);
}
