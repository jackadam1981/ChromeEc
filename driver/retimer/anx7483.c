/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7483: Active redriver with linear equilzation
 */

#include "anx7483.h"
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
#define ANX7483_I2C_WAKE_TIMEOUT_MS	30
#define ANX7483_I2C_WAKE_RETRY_DELAY_US 5000

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static inline int anx7483_read(const struct usb_mux *me,
			       uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static inline int anx7483_write(const struct usb_mux *me,
				uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7483_init(const struct usb_mux *me)
{
	timestamp_t start;
	int rv;
	int val;

	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	/* Keep reading control register until mux wakes up or times out */
	start = get_time();
	do {
		rv = anx7483_read(me, ANX7483_ANALOG_STATUS_CTRL_REG, &val);
		if (!rv)
			break;
		usleep(ANX7483_I2C_WAKE_RETRY_DELAY_US);
	} while (time_since32(start) < ANX7483_I2C_WAKE_TIMEOUT_MS * MSEC);

	if (rv) {
		CPRINTS("ANX7483: Failed to wake mux rv:%d", rv);
		return EC_ERROR_TIMEOUT;
	}

	/* Configure for i2c control */
	val |= ANX7483_CTRL_REG_EN;
	RETURN_ERROR(anx7483_write(me, ANX7483_ANALOG_STATUS_CTRL_REG, val));

	return EC_SUCCESS;
}

static int anx7483_set(const struct usb_mux *me, mux_state_t mux_state,
		       bool *ack_required)
{
	int reg;

	/* This driver does not use host command ACKs */
	*ack_required = false;

	/*
	 * Mux is not powered in Z1
	 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return EC_ERROR_NOT_POWERED;

	/*
	 * Always ensure i2c control is set and state machine is enabled
	 * (setting ANX7483_CTRL_REG_BYPASS_EN disables state machine)
	 */
	reg = ANX7483_CTRL_REG_EN;
	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= ANX7483_CTRL_USB_EN;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= ANX7483_CTRL_DP_EN;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= ANX7483_CTRL_FLIP_EN;

	return anx7483_write(me, ANX7483_ANALOG_STATUS_CTRL_REG, reg);
}

static int anx7483_get(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;

	/* Mux is not powered in Z1 */
	if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
		return USB_PD_MUX_NONE;

	*mux_state = 0;
	RETURN_ERROR(anx7483_read(me, ANX7483_ANALOG_STATUS_CTRL_REG, &reg));

	if (reg & ANX7483_CTRL_USB_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & ANX7483_CTRL_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & ANX7483_CTRL_FLIP_EN)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

enum ec_error_list anx7483_set_default_tuning(const struct usb_mux *me,
					      mux_state_t mux_state)
{
	bool flipped = mux_state & USB_PD_MUX_POLARITY_INVERTED;

	/* Remove flipped from the state for easier compraisons */
	mux_state = mux_state & ~USB_PD_MUX_POLARITY_INVERTED;

	/* Enable i2c configuration of tuning registers */
	RETURN_ERROR(anx7483_write(me, ANX7483_ENABLE_EQ_FLAT_SWING_REG,
				   ANX7483_ENABLE_EQ_FLAT_SWING_EN));

	if (mux_state == USB_PD_MUX_USB_ENABLED) {
		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7E));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7E));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7E));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7E));

		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
	} else if (mux_state == USB_PD_MUX_DP_ENABLED) {
		RETURN_ERROR(anx7483_write(me, ANX7483_AUX_SNOOPING_CTRL_REG,
					   ANX7483_AUX_SNOOPING_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
	} else if (mux_state == USB_PD_MUX_DOCK && !flipped) {
		RETURN_ERROR(anx7483_write(me, ANX7483_AUX_SNOOPING_CTRL_REG,
					   ANX7483_AUX_SNOOPING_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
	} else if (mux_state == USB_PD_MUX_DOCK && flipped) {
		RETURN_ERROR(anx7483_write(me, ANX7483_AUX_SNOOPING_CTRL_REG,
					   ANX7483_AUX_SNOOPING_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG2_REG,
					   ANX7483_CFG2_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG0_REG,
					   ANX7483_CFG0_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_ENABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG4_REG,
					   ANX7483_CFG4_TERM_DISABLE));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG1_REG,
					   ANX7483_CFG1_DEF));

		RETURN_ERROR(anx7483_write(me, ANX7483_URX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_URX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_UTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DRX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX1_PORT_CFG3_REG,
					   ANX7483_CFG3_3A));
		RETURN_ERROR(anx7483_write(me, ANX7483_DTX2_PORT_CFG3_REG,
					   ANX7483_CFG3_7A));
	}

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7483_usb_retimer_driver = {
	.init = anx7483_init,
	.set = anx7483_set,
	.get = anx7483_get,
};
