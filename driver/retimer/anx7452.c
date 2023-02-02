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
 * Programming guide specifies it may be as much as 50ms after chip power on
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
		rv = anx7483_read(me, ANX7452_TOP_CTRL_REG, &val);
		if (!rv)
			break;
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);

	if (rv) {
		CPRINTS("ANX7452: Failed to wake mux rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

	/* Configure for i2c control */
	val |= ANX7452_TOP_CTRL_REG_EN;
	RETURN_ERROR(anx7483_write(me, ANX7452_TOP_CTRL_REG, val));

	return EC_SUCCESS;
}

static int anx7452_set(const struct usb_mux *me, mux_state_t mux_state,
		       bool *ack_required)
{
	int reg;
	int rv;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	/*
	 * Mux is not powered in Z1
	 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	reg = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_CTRL_REG, &reg));
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		reg |= ANX7452_TOP_CTRL_FLIP_EN;
	}
	else {
		reg &= ~ANX7452_TOP_CTRL_FLIP_EN;
	}
	rv = anx7483_write(me, ANX7452_TOP_CTRL_REG, reg);
	if (rv) {
		CPRINTS("ANX7452: Failed to write flip status to ANX7452_TOP_CTRL_REG rv:%d",
			rv);
		return EC_ERROR_TIMEOUT;
	}

	reg = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_CTLTOP_FLIP_REG, &reg));
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		reg |= ANX7452_CTRL_FLIP_EN;
	}
	else {
		reg &= ~ANX7452_CTRL_FLIP_EN;
	}
	rv = anx7483_write(me, ANX7452_CTLTOP_FLIP_REG, reg);
	if (rv) {
		CPRINTS("ANX7452: Failed to write flip status to ANX7452_CTLTOP_FLIP_REG rv:%d",
			rv);
		return EC_ERROR_TIMEOUT;
	}

	if (mux_state & USB_PD_MUX_DP_ENABLED) {
		reg = 0;
		RETURN_ERROR(anx7452_read(me, ANX7452_CTLTOP_DP_REG, &reg));
		reg |= ANX7452_CTRL_DP_EN;
		rv = anx7483_write(me, ANX7452_CTLTOP_DP_REG, reg);
		if (rv) {
			CPRINTS("ANX7452: Failed to write to ANX7452_CTLTOP_DP_REG rv:%d",
			rv);
			return EC_ERROR_TIMEOUT;
		}
	}

	if (mux_state & USB_PD_MUX_USB_ENABLED) {
		reg = 0;
		RETURN_ERROR(anx7452_read(me, ANX7452_CTLTOP_USB3_REG, &reg));
		reg |= ANX7452_CTRL_USB3_EN;
		rv = anx7483_write(me, ANX7452_CTLTOP_USB3_REG, reg);
		if (rv) {
			CPRINTS("ANX7452: Failed to write to ANX7452_CTLTOP_USB3_REG rv:%d",
			rv);
			return EC_ERROR_TIMEOUT;
		}
	}

	if (mux_state & USB_PD_MUX_USB4_ENABLED) {
		reg = 0;
		RETURN_ERROR(anx7452_read(me, ANX7452_CTLTOP_USB4_REG, &reg));
		reg |= ANX7452_CTRL_USB4_EN;
		rv = anx7483_write(me, ANX7452_CTLTOP_USB4_REG, reg);
		if (rv) {
			CPRINTS("ANX7452: Failed to write to ANX7452_CTLTOP_USB4_REG rv:%d",
			rv);
			return EC_ERROR_TIMEOUT;
		}
	}

	if (mux_state & USB_PD_MUX_TBT_COMPAT_ENABLED) {
		reg = 0;
		RETURN_ERROR(anx7452_read(me, ANX7452_CTLTOP_TBT_REG, &reg));
		reg |= ANX7452_CTRL_TBT_EN;
		rv = anx7483_write(me, ANX7452_CTLTOP_TBT_REG, reg);
		if (rv) {
			CPRINTS("ANX7452: Failed to write to ANX7452_CTLTOP_TBT_REG rv:%d",
			rv);
			return EC_ERROR_TIMEOUT;
		}
	}

	return 0;
}

static int anx7452_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg = 0;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return USB_PD_MUX_NONE;

	*mux_state = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_CTRL_REG, &reg));
	if(reg & ANX7452_TOP_CTRL_FLIP_EN){
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;
	}
	else{
		*mux_state &= ~USB_PD_MUX_POLARITY_INVERTED;
	}
	if (reg & ANX7452_TOP_CTRL_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & ANX7452_TOP_CTRL_TBT_EN)
		*mux_state |= USB_PD_MUX_TBT_COMPAT_ENABLED;
	if (reg & ANX7452_TOP_CTRL_USB3_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & ANX7452_TOP_CTRL_USB4_EN)
		*mux_state |= USB_PD_MUX_USB4_ENABLED;

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7452_usb_retimer_driver = {
	.init = anx7452_init,
	.set = anx7452_set,
	.get = anx7452_get,
};
