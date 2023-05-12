/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7452: Active redriver with linear equalisation
 */

#include "anx7452.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "retimer/anx7452_public.h"
#include "timer.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ##args)

static int slow_read8(const int port, const uint16_t addr_flags, int reg, int *val)
{
	timestamp_t start;
	int rv;

	start = get_time();
	do {
		rv = i2c_read8(port, addr_flags, reg, val);
		if (!rv)
			break;
		usleep(ANX7452_I2C_WAKE_RETRY_DELAY_US);
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);
	if (rv) {
		ccprintf("%s: read reg 0x%02x failed: %d\n", __func__, reg, rv);
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

static int anx7452_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return slow_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int slow_write8(const int port, const uint16_t addr_flags, int reg, int val)
{
	timestamp_t start;
	int rv;

	start = get_time();
	do {
		rv = i2c_write8(port, addr_flags, reg, val);
		if (!rv)
			break;
		usleep(ANX7452_I2C_WAKE_RETRY_DELAY_US);
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);
	if (rv) {
		ccprintf("%s: write reg 0x%02x failed: %d\n", __func__, reg, rv);
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

static int anx7452_write(const struct usb_mux *me, uint8_t reg, uint8_t val)
{
	return slow_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7452_ctltop_update(const struct usb_mux *me, uint8_t reg,
				 uint8_t mask, uint8_t val)
{
	int reg_val = 0;
	int rv;

	RETURN_ERROR(slow_read8(me->i2c_port, ANX7452_I2C_ADDR_CTLTOP_FLAGS, reg,
				&reg_val));
	reg_val = (reg_val & ~mask) | (val & mask);
	rv = slow_write8(me->i2c_port, ANX7452_I2C_ADDR_CTLTOP_FLAGS, reg,
			 reg_val);
	if (rv) {
		CPRINTS("ANX7452: Failed to write to ctltop register %x rv:%d",
			reg, rv);
		return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}

static int anx7452_ctltop_update_all(const struct usb_mux *me, uint8_t cfg0_val,
				     uint8_t cfg1_val, uint8_t cfg2_val)
{
	RETURN_ERROR(anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG0_REG,
					   ANX7452_CTLTOP_CFG0_REG_BIT_MASK,
					   cfg0_val));
	RETURN_ERROR(anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG1_REG,
					   ANX7452_CTLTOP_CFG1_REG_BIT_MASK,
					   cfg1_val));
	RETURN_ERROR(anx7452_ctltop_update(me, ANX7452_CTLTOP_CFG2_REG,
					   ANX7452_CTLTOP_CFG2_REG_BIT_MASK,
					   cfg2_val));

//	RETURN_ERROR(anx7452_write(me, ANX7452_TOP_STATUS_REG,
//				   ANX7452_TOP_REG_EN));

	return EC_SUCCESS;
}

static int anx7452_wake_up(const struct usb_mux *me)
{
	timestamp_t start;
	int rv;
	int val;

	/* Keep reading top register until mux wakes up or timesout */
	start = get_time();
	do {
		//rv = anx7452_read(me, ANX7452_TOP_STATUS_REG/*0x00*/, &val);
		rv = anx7452_read(me, 0x00, &val);
		if (!rv)
			break;
		usleep(ANX7452_I2C_WAKE_RETRY_DELAY_US);
	} while (time_since32(start) < ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);
	if (rv) {
		CPRINTS("ANX7452: Failed to wake mux rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

//	usleep(ANX7452_I2C_WAKE_TIMEOUT_MS * MSEC);

//	CPRINTS("ANX7452: woke after %d ms", time_since32(start) / MSEC);

	/* ULTRA_LOW_POWER must always be disabled (Fig 2-2) */
//	RETURN_ERROR(anx7452_write(me, 0xe6/*ANX7451_REG_ULTRA_LOW_POWER*/,
//				   0x00/*ANX7451_ULTRA_LOW_POWER_DIS*/));

//	ccprintf("%s: ULP off\n", __func__);

	return EC_SUCCESS;
}

static int anx7452_init(const struct usb_mux *me)
{
//	int usb_enable;

	ccprintf("%s: call\n", __func__);

//	usb_enable = anx7452_controls[me->usb_port].usb_enable_gpio;
//	gpio_set_level(usb_enable, 1);

	RETURN_ERROR(anx7452_wake_up(me));

	/* Configure for i2c control */
//	RETURN_ERROR(anx7452_write(me, ANX7452_TOP_STATUS_REG,
//				   ANX7452_TOP_REG_EN));

	ccprintf("%s: done\n", __func__);

	return EC_SUCCESS;
}

static int anx7452_set(const struct usb_mux *me, mux_state_t mux_state,
		       bool *ack_required)
{
	int cfg0_val = 0;
	int cfg1_val = 0;
	int cfg2_val = 0;
	int port = me->usb_port;
	int reg;
	int rv;

	ccprintf("%s: mux_state 0x%02x\n", __func__, (int)mux_state);

	/* This driver does not use host command ACKs */
	*ack_required = false;

	/* ================================================================ */

	/* Apply USB3 enable settings */
	if (mux_state & USB_PD_MUX_USB_ENABLED) {
		cfg0_val |= ANX7452_CTLTOP_CFG0_USB3_EN;
	}

	/* Apply DP enable settings */
	if (mux_state & USB_PD_MUX_DP_ENABLED) {
		cfg1_val |= ANX7452_CTLTOP_CFG1_DP_EN;
		/*
		 * pin assignments:
		 *   00 E/E'
		 *   01 C/C'/D/D'
		 *   10 reserved
		 *   11reserved
		 */
		uint8_t dp_pin_mode = get_dp_pin_mode(port);
		switch (dp_pin_mode) {
		case MODE_DP_PIN_E:
			cfg1_val |= ANX7452_CTLTOP_CFG1_DP_PM_E;
			break;
		case MODE_DP_PIN_C:
		case MODE_DP_PIN_D:
			cfg1_val |= ANX7452_CTLTOP_CFG1_DP_PM_C_D;
			break;
		}
	}

	/* Apply CC polarity settings */
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		cfg0_val |= ANX7452_CTLTOP_CFG0_FLIP_EN;
	}

	/* Apply HPD IRQ settings */
	if (mux_state & USB_PD_MUX_HPD_IRQ) {
		cfg1_val |= ANX7452_CTLTOP_CFG1_IRQ_HPD;
	}

	/* Apply HPD level settings */
	if (mux_state & USB_PD_MUX_HPD_LVL) {
		cfg1_val |= ANX7452_CTLTOP_CFG1_HPD_LVL;
	}

	/* Apply TBT compatible enable settings */
	if (mux_state & USB_PD_MUX_TBT_COMPAT_ENABLED) {
		cfg2_val |= ANX7452_CTLTOP_CFG2_TBT_EN;
	}

	/* Apply USB4 enable settings */
	if (mux_state & USB_PD_MUX_USB4_ENABLED) {
		cfg2_val |= ANX7452_CTLTOP_CFG2_USB4_EN;
	}

	RETURN_ERROR(anx7452_wake_up(me));

	/* Configure for i2c control */
	rv = anx7452_read(me, ANX7452_TOP_STATUS_REG,
				   &reg);
	if (rv != EC_SUCCESS) {
		ccprintf("%s: read SR failed: %d\n", __func__, rv);
		return rv;
	}
	reg |= ANX7452_TOP_REG_EN;
	rv = anx7452_write(me, ANX7452_TOP_STATUS_REG,
			   reg/*ANX7452_TOP_REG_EN*/);
	if (rv != EC_SUCCESS) {
		ccprintf("%s: REG_EN failed: %d\n", __func__, rv);
		return rv;
	}

	rv = anx7452_ctltop_update_all(me, cfg0_val, cfg1_val, cfg2_val);
	if (rv != EC_SUCCESS) {
		ccprintf("%s: update all failed: %d\n", __func__, rv);
		return rv;
	}

	ccprintf("%s: mux_state 0x%02x done\n", __func__, (int)mux_state);

	return rv;
}

static int anx7452_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg = 0;
	int tsr;

	ccprintf("%s: call\n", __func__);

	RETURN_ERROR(anx7452_wake_up(me));

	*mux_state = 0;
	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_STATUS_REG, &reg));
	if (reg & ANX7452_TOP_FLIP_INFO) {
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;
	}
	if (reg & ANX7452_TOP_DP_INFO) {
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	}
	if (reg & ANX7452_TOP_TBT_INFO) {
		*mux_state |= USB_PD_MUX_TBT_COMPAT_ENABLED;
	}
	if (reg & ANX7452_TOP_USB3_INFO) {
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	}
	if (reg & ANX7452_TOP_USB4_INFO) {
		*mux_state |= USB_PD_MUX_USB4_ENABLED;
	}

	RETURN_ERROR(anx7452_read(me, ANX7452_TOP_STATUS_REG, &tsr));

	ccprintf("%s: mux_state 0x%02x, TSR 0x%02x\n", __func__,
		 (int)*mux_state, tsr);

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7452_usb_retimer_driver = {
	.init = anx7452_init,
	.set = anx7452_set,
	.get = anx7452_get,
};
