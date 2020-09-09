/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IT5205 Type-C SBU OVP handler
 */

#include "console.h"
#include "hooks.h"
#include "it5205.h"
#include "stdbool.h"
#include "timer.h"
#include "usb_mux.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define OVP_RETRY_DELAY_US_MIN (100 * MSEC)

static unsigned int ovp_retry_delay_us = OVP_RETRY_DELAY_US_MIN;

static void reset_retry_delay(void)
{
	CPRINTS("IT5205 SBU OVP cleared");
	ovp_retry_delay_us = OVP_RETRY_DELAY_US_MIN;
}
DECLARE_DEFERRED(reset_retry_delay);

static void reset_csbu(void)
{
	/* double the retry time upto 1 minute */
	ovp_retry_delay_us = MIN(ovp_retry_delay_us * 2, MINUTE);
	/* and reset it if interrupt not triggered in a short period */
	hook_call_deferred(&reset_retry_delay_data, 500 * MSEC);

	/* re-enable sbu interrupt */
	it5205h_enable_csbu_switch(&usb_muxes[0], false);
	it5205h_enable_csbu_switch(&usb_muxes[0], true);
}
DECLARE_DEFERRED(reset_csbu);

static void it5205h_pd_disconnect(void)
{
	int reg;

	if (i2c_read8(I2C_PORT_USB_MUX0, IT5205H_SBU_I2C_ADDR_FLAGS,
		      IT5205H_REG_ISR, &reg))
		return;

	/*
	 * Re-poll ovp status immidiately if pd disconnected, to avoid the
	 * long delay.
	 * This hook might trigger by the unrelated port 1 on sub-board, but
	 * it's fine since we are just shortning the retry interval.
	 */
	if (reg & IT5205H_ISR_CSBU_OVP)
		hook_call_deferred(&reset_csbu_data, 0);
}
DECLARE_HOOK(HOOK_USB_PD_DISCONNECT, it5205h_pd_disconnect, HOOK_PRIO_DEFAULT);

void it5205h_sbu_interrupt(enum gpio_signal signal)
{
	CPRINTS("IT5205 SBU OVP triggered");
	hook_call_deferred(&reset_csbu_data, ovp_retry_delay_us);
	hook_call_deferred(&reset_retry_delay_data, -1);
}
