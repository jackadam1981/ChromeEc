/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7452: Active redriver with linear equilzation
 */

#include "anx7452.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "i2c.h"
#include "timer.h"
#include "usb_mux.h"
#include "util.h"

/*
 * Programming guide specifies it may be as much as 30ms after chip power on
 * before it's ready for i2c
 */
#define ANX7452_I2C_WAKE_TIMEOUT_MS 50

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)

static inline int anx7452_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static inline int anx7452_write(const struct usb_mux *me, uint8_t reg,
				uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7452_init(const struct usb_mux *me)
{
	timestamp_t start;
	int rv;
	int val;

	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	/* Keep reading control register until mux wakes up or times out */
	start = get_time();
	do {
		rv = anx7483_read(me, ANX7452_TOP, &val);
		if (!rv)
			break;
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);

	if (rv) {
		CPRINTS("ANX7452: Failed to wake mux rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

	return EC_SUCCESS;
}

static int anx7452_set(const struct usb_mux *me, mux_state_t mux_state,
		       bool *ack_required)
{
	int top_reg, ctrl_top_reg1, ctrl_top_reg2, ctrl_top_reg3;
	int rv;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	/*
	 * Mux is not powered in Z1
	 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	RETURN_ERROR(anx7452_read(me, ANX7452_TOP, &top_reg));
	// TODO : If the SWAP mode is enabled, that is when chip’s UFP port is
	//  swapped with DFP port, then [TOP:0xF8] bit5 shall set to 1
	// not sure what to do here

	RETURN_ERROR(anx7452_read(me, ANX7452_CTRL_TOP_1, &ctrl_top_reg1));
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		ctrl_top_reg_1 |=
			ANX7452_CTRL_TOP_1_FLIP_EN if (mux_state &
						       USB_PD_MUX_USB_ENABLED)
				ctrl_top_reg_1 |= ANX7452_CTRL_TOP_1_USB3_EN;

	RETURN_ERROR(anx7452_read(me, ANX7452_CTRL_TOP_2, &ctrl_top_reg2));
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		ctrl_top_reg_2 |= ANX7452_CTRL_TOP_2_DP_EN;

	RETURN_ERROR(anx7452_read(me, ANX7452_CTRL_TOP_3, &ctrl_top_reg3));
	if (mux_state & USB_PD_MUX_USB4_ENABLED)
		ctrl_top_reg_3 |= ANX7452_CTRL_TOP_3_USB4_EN;
	if (mux_state & USB_PD_MUX_TBT_COMPAT_ENABLED)
		ctrl_top_reg_3 |= ANX7452_CTRL_TOP_3_TBT_EN;

	rv = anx7483_write(me, ANX7452_TOP, top_reg);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ANX7452_TOP rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

	rv = anx7483_write(me, ANX7452_CTRL_TOP_1, ctrl_top_reg_1);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ANX7452_CTRL_TOP_1 rv:%d",
			rv);
		return EC_ERROR_TIMEOUT;
	}

	rv = anx7483_write(me, ANX7452_CTRL_TOP_2, ctrl_top_reg_2);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ANX7452_CTRL_TOP_2 rv:%d",
			rv);
		return EC_ERROR_TIMEOUT;
	}

	rv = anx7483_write(me, ANX7452_CTRL_TOP_3, ctrl_top_reg_3);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ANX7452_CTRL_TOP_3 rv:%d",
			rv);
		return EC_ERROR_TIMEOUT;
	}

	return 0;
}

static int anx7452_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return USB_PD_MUX_NONE;

	*mux_state = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP, &reg));

	if (reg & ANX7452_TOP_SWAP_EN)
		*mux_state |= 0; // not sure what to do here
	if (reg & ANX7452_TOP_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & ANX7452_TOP_FLIP_EN)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;
	if (reg & ANX7452_TOP_TBT_EN)
		*mux_state |= USB_PD_MUX_TBT_COMPAT_ENABLED;
	if (reg & ANX7452_TOP_USB3_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & ANX7452_TOP_USB4_EN)
		*mux_state |= USB_PD_MUX_USB4_ENABLED;

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7452_usb_retimer_driver = {
	.init = anx7452_init,
	.set = anx7452_set,
	.get = anx7452_get,
};
