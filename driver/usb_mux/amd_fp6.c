/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMD FP6 USB/DP Mux.
 */

#include "amd_fp6.h"
#include "chipset.h"
#include "common.h"
#include "hooks.h"
#include "i2c.h"
#include "queue.h"
#include "timer.h"
#include "usb_mux.h"

/*
 * Local data structure for saving mux state so it can be restored after
 * an AP reset.
 */
static struct {
	const struct usb_mux *mux;
	mux_state_t state;
} saved_mux_state[USBC_PORT_COUNT];

static inline int amd_fp6_mux_read(const struct usb_mux *me, uint8_t *val)
{
	uint8_t buf[3] = { 0 };
	int rv;
	bool mux_ready;

	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;


	rv = i2c_xfer(me->i2c_port, me->i2c_addr_flags, NULL, 0, buf, 3);
	if (rv)
		return rv;

	/*
	 * Payload Byte 0 = Status/ID
	 * Payload Byte 1 = Index 0 Control/Status
	 * Payload Byte 2 = Index 1 Control/Status
	 */
	mux_ready = !!(buf[0] & AMD_FP6_MUX_STATUS_READY);
	if (!mux_ready)
		return EC_ERROR_BUSY;

	*val = buf[1];
	return EC_SUCCESS;
}

static inline int amd_fp6_mux_write(const struct usb_mux *me, uint8_t val)
{
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	return i2c_write8(me->i2c_port, me->i2c_addr_flags, 0, val);
}

static inline int amd_fp6_init(const struct usb_mux *me)
{
	return EC_SUCCESS;
}

static int amd_fp6_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	uint8_t val;
	uint8_t mode;
	int rv;

	saved_mux_state[me->usb_port].mux = me;
	saved_mux_state[me->usb_port].state = mux_state;

	if ((mux_state & USB_PD_MUX_USB_ENABLED) &&
	    (mux_state & USB_PD_MUX_DP_ENABLED))
		mode = AMD_FP6_MUX_MODE_DOCK;
	else if (mux_state & USB_PD_MUX_USB_ENABLED)
		mode = AMD_FP6_MUX_MODE_USB;
	else if (mux_state & USB_PD_MUX_DP_ENABLED)
		mode = AMD_FP6_MUX_MODE_DOCK;
	else
		mode = AMD_FP6_MUX_MODE_SAFE;

	val = mode << AMD_FP6_MUX_MODE_OFFSET;

	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		val |= AMD_FP6_MUX_ORIENTATION;

	/* Always enable lower_power in SAFE mode */
	if (mode == AMD_FP6_MUX_MODE_SAFE)
		val |= AMD_FP6_MUX_LOW_POWER;

	rv = amd_fp6_mux_write(me, val);
	/*
	 * This MUX is on the FP6 SoC.  If that device is not powered then
	 * we either have to complain that it is not powered or if we were
	 * setting the state to OFF, then go ahead and report that we did
	 * it because a powered down MUX is off.
	 */
	if (rv == EC_ERROR_NOT_POWERED && mux_state == USB_PD_MUX_NONE)
		rv = EC_SUCCESS;
	return rv;
}

static int amd_fp6_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	uint8_t val;
	bool inverted;
	uint8_t mode;
	int rv;

	rv = amd_fp6_mux_read(me, &val);
	/*
	 * This MUX is on the FP6 SoC. If that device is not powered then claim
	 * thestate to be NONE, which is SAFE.
	 */
	if (rv == EC_ERROR_NOT_POWERED)
		val = 0;
	else if (rv)
		return rv;

	mode = (val & AMD_FP6_MUX_MODE_MASK) >> AMD_FP6_MUX_MODE_OFFSET;
	inverted = !!(val & AMD_FP6_MUX_ORIENTATION);

	if (mode == AMD_FP6_MUX_MODE_USB)
		*mux_state = USB_PD_MUX_USB_ENABLED;
	else if (mode == AMD_FP6_MUX_MODE_DP)
		*mux_state = USB_PD_MUX_DP_ENABLED;
	else if (mode == AMD_FP6_MUX_MODE_DOCK)
		*mux_state = USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED;
	else /* AMD_FP6_MUX_MODE_SAFE */
		*mux_state = USB_PD_MUX_NONE;

	if (inverted)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

static void amd_fp6_chipset_reset_delay(void)
{
	int rv;
	int i;

	for (i = 0; i < ARRAY_SIZE(saved_mux_state); i++) {
		/* Check if saved_mux_state has been initialized */
		if (saved_mux_state[i].mux == NULL)
			continue;
		rv = amd_fp6_set_mux(saved_mux_state[i].mux,
				     saved_mux_state[i].state);
		if (rv)
			ccprints("C%d restore mux rv:%d", i, rv);
	}

}
DECLARE_DEFERRED(amd_fp6_chipset_reset_delay);

/*
 * The AP's internal USB-C mux is reset when AP resets, so wait for
 * it to be ready and then restore the previous setting.
 */
static int amd_fp6_chipset_reset(const struct usb_mux *mux)
{
	/* TODO: Tune 200ms delay for FP6 */
	hook_call_deferred(&amd_fp6_chipset_reset_delay_data, 200 * MSEC);
	return EC_SUCCESS;
}

const struct usb_mux_driver amd_fp6_usb_mux_driver = {
	.init = &amd_fp6_init,
	.set = &amd_fp6_set_mux,
	.get = &amd_fp6_get_mux,
	.chipset_reset = &amd_fp6_chipset_reset,
};
