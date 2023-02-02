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

/* CTLTOP I2C register address will be stored in this variable and used by the
 * driver code. It gets assigned with actual value in the init function */
static int ctltop_i2c_reg_addr;

static inline int anx7452_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static inline int anx7452_write(const struct usb_mux *me, uint8_t reg,
				uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static inline int anx7452_ctltop_read(const struct usb_mux *me, uint8_t reg,
				      int *val)
{
	return i2c_read8(me->i2c_port, ctltop_i2c_reg_addr, reg, val);
}

static inline int anx7452_ctltop_write(const struct usb_mux *me, uint8_t reg,
				       uint8_t val)
{
	return i2c_write8(me->i2c_port, ctltop_i2c_reg_addr, reg, val);
}

int anx7452_ctltop_update(const struct usb_mux *me, uint8_t reg, uint8_t mask,
			  uint8_t val)
{
	int reg_val = 0;
	int rv;

	RETURN_ERROR(anx7452_ctltop_read(me, reg, &reg_val));
	reg_val = (reg_val & ~mask) | (val & mask);
	rv = anx7452_ctltop_write(me, reg, reg_val);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ctltop register %d rv:%d",
			reg, rv);
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

int anx7452_ctltop_update_all(const struct usb_mux *me, uint8_t cfg0_val,
			      uint8_t cfg1_val, uint8_t cfg2_val)
{
	int rv0, rv1, rv2;

	rv0 = anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG0_REG,
				    ANX7452_CTLTOP_CFG0_REG_BIT_MASK, cfg0_val);
	rv1 = anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG1_REG,
				    ANX7452_CTLTOP_CFG1_REG_BIT_MASK, cfg1_val);
	rv2 = anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG2_REG,
				    ANX7452_CTLTOP_CFG2_REG_BIT_MASK, cfg2_val);
	if (rv0 || rv1 || rv2) {
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
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
		rv = anx7452_read(me, ANX7452_TOP_REG, &val);
		if (!rv)
			break;
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);

	if (rv) {
		CPRINTS("ANX7452: Failed to wake mux rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

	/* Configure for i2c control */
	val |= ANX7452_TOP_REG_EN;
	RETURN_ERROR(anx7452_write(me, ANX7452_TOP_REG, val));

	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_CTLTOP_I2C_ADDR_REG, &val));
	ctltop_i2c_reg_addr = val >> 1;

	return EC_SUCCESS;
}

static int anx7452_set(const struct usb_mux *me, mux_state_t mux_state,
		       bool *ack_required)
{
	int cfg0_val = 0;
	int cfg1_val = 0;
	int cfg2_val = 0;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	/*
	 * Mux is not powered in Z1
	 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	/* Apply CC polarity settings */
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		cfg0_val |= ANX7452_CTLTOP_CFG0_FLIP_EN;
	} else {
		cfg0_val &= ~ANX7452_CTLTOP_CFG0_FLIP_EN;
	}

	/* Apply DP enable settings */
	if (mux_state & USB_PD_MUX_DP_ENABLED) {
		cfg1_val |= ANX7452_CTLTOP_CFG1_DP_EN;
	}

	/* Apply USB3 enable settings */
	if (mux_state & USB_PD_MUX_USB_ENABLED) {
		cfg0_val |= ANX7452_CTLTOP_CFG0_USB3_EN;
	}

	/* Apply USB4 enable settings */
	if (mux_state & USB_PD_MUX_USB4_ENABLED) {
		cfg2_val |= ANX7452_CTLTOP_CFG2_USB4_EN;
	}

	/* Apply TBT compatible enable settings */
	if (mux_state & USB_PD_MUX_TBT_COMPAT_ENABLED) {
		cfg2_val |= ANX7452_CTLTOP_CFG2_TBT_EN;
	}

	return anx7452_ctltop_update_all(me, cfg0_val, cfg1_val, cfg2_val);
}

static int anx7452_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg = 0;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return USB_PD_MUX_NONE;

	*mux_state = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_REG, &reg));
	if (reg & ANX7452_TOP_FLIP_INFO) {
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;
	} else {
		*mux_state &= ~USB_PD_MUX_POLARITY_INVERTED;
	}
	if (reg & ANX7452_TOP_DP_INFO)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & ANX7452_TOP_TBT_INFO)
		*mux_state |= USB_PD_MUX_TBT_COMPAT_ENABLED;
	if (reg & ANX7452_TOP_USB3_INFO)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & ANX7452_TOP_USB4_INFO)
		*mux_state |= USB_PD_MUX_USB4_ENABLED;

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7452_usb_retimer_driver = {
	.init = anx7452_init,
	.set = anx7452_set,
	.get = anx7452_get,
};
