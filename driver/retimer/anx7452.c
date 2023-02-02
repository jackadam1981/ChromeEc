/* Copyright 2023 The ChromiumOS Authors
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

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)

static int anx7452_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7452_write(const struct usb_mux *me, uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7452_ctltop_update(const struct usb_mux *me, uint8_t i2c_addr,
				 uint8_t reg, uint8_t mask, uint8_t val)
{
	int reg_val = 0;
	int rv;

	RETURN_ERROR(i2c_read8(me->i2c_port, i2c_addr, reg, &reg_val));
	reg_val = (reg_val & ~mask) | (val & mask);
	rv = i2c_write8(me->i2c_port, i2c_addr, reg, reg_val);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ctltop register %d rv:%d",
			reg, rv);
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

static int anx7452_ctltop_update_all(const struct usb_mux *me, uint8_t cfg0_val,
				     uint8_t cfg1_val, uint8_t cfg2_val)
{
	int ctltop_i2c_addr;

	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_CTLTOP_I2C_ADDR_REG,
				  &ctltop_i2c_addr));
	ctltop_i2c_addr >>= 1;

	RETURN_ERROR(anx7452_ctltop_update(
		me, ctltop_i2c_addr, ANX7452_CTLTOP_CFG0_REG,
		ANX7452_CTLTOP_CFG0_REG_BIT_MASK, cfg0_val));
	RETURN_ERROR(anx7452_ctltop_update(
		me, ctltop_i2c_addr, ANX7452_CTLTOP_CFG1_REG,
		ANX7452_CTLTOP_CFG1_REG_BIT_MASK, cfg1_val));
	RETURN_ERROR(anx7452_ctltop_update(
		me, ctltop_i2c_addr, ANX7452_CTLTOP_CFG2_REG,
		ANX7452_CTLTOP_CFG2_REG_BIT_MASK, cfg2_val));

	return EC_SUCCESS;
}

static int anx7452_init(const struct usb_mux *me)
{
	int val;
	int rv;

	/* Configure for i2c control */
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_REG, &val));
	val |= ANX7452_TOP_REG_EN;
	rv = anx7452_write(me, ANX7452_TOP_REG, val);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to top register %d rv:%d",
			ANX7452_TOP_REG, rv);
		return EC_ERROR_TIMEOUT;
	}

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

	/* Apply CC polarity settings */
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		cfg0_val |= ANX7452_CTLTOP_CFG0_FLIP_EN;
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

	*mux_state = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_REG, &reg));
	if (reg & ANX7452_TOP_FLIP_INFO)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;
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
