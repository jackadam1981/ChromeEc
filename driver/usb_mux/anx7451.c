/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ANX7451: 10G Active Mux (4x4) with
 * Integrated Re-timers for USB3.2/DisplayPort
 */

#include "anx7451.h"
#include "common.h"
#include "console.h"
#include "i2c.h"
#include "time.h"
#include "usb_mux.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static inline int anx7451_read(const struct usb_mux *me,
			       uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static inline int anx7451_write(const struct usb_mux *me,
				uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags, reg, val);
}

static int anx7451_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	int reg;

	/* b/184907521: If both DP and USB disabled, mux will fail */
	if (!(mux_state & (USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED))) {
		CPRINTS("ANX7451 requires USB or DP to be set, "
			"forcing USB enabled");
		mux_state |= USB_PD_MUX_USB_ENABLED;
	}

	/* ULP_CFG_MODE_EN overrides pin control. Always set it */
	reg = ANX7451_ULP_CFG_MODE_EN;
	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= ANX7451_ULP_CFG_MODE_USB_EN;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= ANX7451_ULP_CFG_MODE_DP_EN;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= ANX7451_ULP_CFG_MODE_FLIP;

	return anx7451_write(me, ANX7451_REG_ULP_CFG_MODE, reg);
}

static int anx7451_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;

	*mux_state = 0;
	RETURN_ERROR(anx7451_read(me, ANX7451_REG_ULP_CFG_MODE, &reg));

	if (reg & ANX7451_ULP_CFG_MODE_USB_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & ANX7451_ULP_CFG_MODE_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & ANX7451_ULP_CFG_MODE_FLIP)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

static int anx7451_init(const struct usb_mux *me)
{
	int rv;
	timestamp_t start;

	/* Wait up to 100ms for ANX7451 to be ready */
	start = get_time();
	do {
		int val;

		rv = anx7451_read(me, ANX7451_REG_ULP_CFG_MODE, &val);
	} while (rv && time_since32(start) < ANX7451_I2C_READY_DELAY_MS*MSEC);

	if (rv) {
		CPRINTS("ANX7451 still unresponsive after %d ms",
			ANX7451_I2C_READY_DELAY_MS);
		return EC_ERROR_TIMEOUT;
	}

	/* ULTRA_LOW_POWER must always be disabled (Fig 2-2) */
	RETURN_ERROR(anx7451_write(me, ANX7451_REG_ULTRA_LOW_POWER,
				   ANX7451_ULTRA_LOW_POWER_DIS));

	/*
	 * Start mux in USB mode
	 * b/184907521: None or safe mode causes mux to fail
	 */
	RETURN_ERROR(anx7451_set_mux(me, USB_PD_MUX_USB_ENABLED));

	return EC_SUCCESS;
}

const struct usb_mux_driver anx7451_usb_mux_driver = {
	.init = anx7451_init,
	.set = anx7451_set_mux,
	.get = anx7451_get_mux
	/* Low power mode is not supported on ANX7451 */
};
