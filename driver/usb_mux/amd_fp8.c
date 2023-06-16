/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMD FP8 USB/DP/USB4 Mux.
 */

#include "amd_fp8.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "queue.h"
#include "timer.h"
#include "usb_mux.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

/*
 * Save the last mux state for restoration on AP reset
 */
static mux_state_t saved_mux_state[CONFIG_USB_PD_PORT_MAX_COUNT];

/*
 * Guide dictates that we maintain an "internal mirror" of the crossbar ready
 * status
 */
/* TODO(b/276335130): Add interrupt handler to set this */
static bool xbar_ready[CONFIG_USB_PD_PORT_MAX_COUNT];

static int amd_fp8_mux_read(const struct usb_mux *me, uint8_t command,
				  uint8_t len, uint8_t *buf)
{
	RETURN_ERROR(i2c_xfer(me->i2c_port, me->i2c_addr_flags, &command, 1,
			      buf, len));
}

static int amd_fp8_mux_write(const struct usb_mux *me, uint8_t len,
			     uint8_t *buf)
{
	if (!xbar_ready[me->usb_port])
		return EC_ERROR_BUSY;

	RETURN_ERROR(
		i2c_xfer(me->i2c_port, me->i2c_addr_flags, buf, len, NULL, 0));
}

static int amd_fp8_set_mux(const struct usb_mux *me, mux_state_t mux_state,
			   bool *ack_required)
{
	uint8_t write_bytes[5];
	uint8_t val;

	/* This driver does require host command ACKs */
	*ack_required = true;

	/* This driver treats safe mode as none */
	if (mux_state & USB_PD_MUX_SAFE_MODE)
		mux_state = USB_PD_MUX_NONE;

	/* Index is 0 for all modes */
	write_bytes[AMD_FP8_MUX_WRITE1_INDEX_BYTE] = 0;

	if (mux_state == USB_PD_MUX_NONE) {
		val = AMD_FP8_CONTROL_SAFE;
	} else if ((mux_state & USB_PD_MUX_USB_ENABLED) &&
		 (mux_state & USB_PD_MUX_DP_ENABLED)) {
		val = AMD_FP8_CONTROL_DOCK;
	} else if (mux_state & USB_PD_MUX_USB_ENABLED) {
		val = AMD_FP8_CONTROL_USB;
	} else if (mux_state & USB_PD_MUX_DP_ENABLED) {
		val = AMD_FP8_CONTROL_DP;
	} else {
		/* TODO(b/276335130): Add USB4 and TBT3 handling */
		CPRINTSUSB("C%d: unhandled mux_state %x\n", me->usb_port,
			   mux_state);
		return EC_ERROR_INVAL;
	}

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		val |= AMD_FP8_MUX_W1_CTRL_FLIP;

	/* Save our mux state now that it's passed error checks */
	saved_mux_state[me->usb_port] = mux_state;

	/* TODO(b/276335130): Add Data reset request */

	if (pd_get_data_role(me->usb_port) == PD_ROLE_UFP)
		val |= AMD_FP8_MUX_W1_CTRL_UFP;

	write_bytes[AMD_FP8_MUX_WRITE1_CONTROL_BYTE] = val;

	/* TODO(b/276335130): Add Cable information */
	write_bytes[AMD_FP8_MUX_WRITE1_CABLE_BYTE] = 0;
	write_bytes[AMD_FP8_MUX_WRITE1_VER_BYTE] = 0;

	if (pd_is_connected(me->usb_port))
		write_bytes[AMD_FP8_MUX_WRITE1_SPEED_BYTE] =
			AMD_FP8_MUX_W1_SPEED_TC;
	else
		write_bytes[AMD_FP8_MUX_WRITE1_SPEED_BYTE] = 0;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF)) {
		/* We won't be getting any ACK's from the SoC */
		*ack_required = false;
		return (mux_state == USB_PD_MUX_NONE) ? EC_SUCCESS :
							EC_ERROR_NOT_POWERED;
	}

	return amd_fp8_mux_write(me, ARRAY_SIZE(write_bytes), &write_bytes);
}

static int amd_fp8_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	uint8_t val;
	bool inverted;
	uint8_t mode;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return USB_PD_MUX_NONE;

	/*
	 * TODO(b/276335130): Update to FP8 reg translation
	 * Do we need to save state from the interrupt to reference?  Reading
	 * interferes with interrupt servicing potentially
	 */

	return EC_SUCCESS;
}

/*
 * The FP8 USB Mux will not be ready for writing until *sometime* after S0.
 */
static void amd_fp6_chipset_resume(void)
{
	for (int i = 0; i < ARRAY_SIZE(saved_mux_state); i++) {
		mux_state_t state = saved_mux_state[i];
		/* Queue this up for the mux task */
		usb_mux_set(i, state, (state == USB_PD_MUX_NONE ?
						    USB_SWITCH_DISCONNECT :
						    USB_SWITCH_CONNECT),
			    (state & USB_PD_MUX_POLARITY_INVERTED));
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, amd_fp6_chipset_resume, HOOK_PRIO_DEFAULT);

static int amd_fp8_chipset_reset(const struct usb_mux *me)
{
	if (!chipset_in_or_transitioning_to_state(CHIPSET_STATE_ON))
		return EC_SUCCESS;

	/* Will this double-resume?  Maybe filter by me->usb_port */
	amd_fp6_chipset_resume();
	return EC_SUCCESS;
}

const struct usb_mux_driver amd_fp8_usb_mux_driver = {
	.set = &amd_fp8_set_mux,
	.get = &amd_fp8_get_mux,
	.chipset_reset = &amd_fp8_chipset_reset
};
