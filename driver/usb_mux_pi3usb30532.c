/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB30532 USB port switch driver.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "pi3usb30532.h"
#include "usb_mux.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Translate enum typec_mux modes to register values */
static const uint8_t mode_to_reg[] = {
		[TYPEC_MUX_NONE] = PI3USB30532_MODE_POWERON,
		[TYPEC_MUX_USB] = PI3USB30532_MODE_USB,
		[TYPEC_MUX_DP] = PI3USB30532_MODE_DP,
		[TYPEC_MUX_DOCK] = PI3USB30532_MODE_DP_USB,
};
BUILD_ASSERT(ARRAY_SIZE(mode_to_reg) == TYPEC_MUX_CONFIG_COUNT);

static int pi3usb30532_read(int i2c_addr, uint8_t reg, uint8_t *val)
{
	int read;

	if (i2c_read8(I2C_PORT_USB_SWITCH, i2c_addr, reg, &read))
		return -1;

	*val = read;
	return 0;
}

static int pi3usb30532_write(int i2c_addr, uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_USB_SWITCH, i2c_addr, reg, val);
	if (res)
		CPUTS("PI3USB30532 I2C write failed");

	return res;
}

static int pi3usb30532_reset(int i2c_addr)
{
	return pi3usb30532_write(
		i2c_addr,
		PI3USB30532_REG_CONTROL,
		(PI3USB30532_MODE_POWERDOWN & PI3USB30532_CTRL_MASK) |
		PI3USB30532_CTRL_RSVD);
}

static int pi3usb30532_init(int i2c_addr)
{
	uint8_t val;

	if (pi3usb30532_reset(i2c_addr)) {
		CPRINTS("PI3USB30532 [%d] init failed", i2c_addr);
		return -1;
	}

	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_VENDOR, &val)) {
		CPRINTS("PI3USB30532 [%d] read failed", i2c_addr);
		return -1;
	} else if (val != PI3USB30532_VENDOR_ID) {
		CPRINTS("PI3USB30532 [%d] invalid ID 0x%02x", i2c_addr, val);
		return -1;
	}

	return 0;
}

/* Reads control register and updates mux_state accordingly */
static int pi3usb30532_get_mux(int i2c_addr, struct usb_mux_state *mux_state)
{
	int i;
	uint8_t reg;

	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_CONTROL, &reg))
		return -1;

	mux_state->polarity_swapped = reg & PI3USB30532_BIT_SWAP;
	mux_state->mux_mode = TYPEC_MUX_UNKNOWN;

	reg &= (PI3USB30532_CTRL_MASK & ~PI3USB30532_BIT_SWAP);
	for (i = 0; i < ARRAY_SIZE(mode_to_reg); ++i)
		if (mode_to_reg[i] == reg)
			mux_state->mux_mode = i;
	return 0;
}

/* Writes control register to set switch mode */
static int pi3usb30532_set_mux(int i2c_addr,
			       struct usb_mux_state *mux_state)
{
	uint8_t reg;

	reg = mode_to_reg[mux_state->mux_mode];
	if (mux_state->polarity_swapped)
		reg |= PI3USB30532_BIT_SWAP;

	return pi3usb30532_write(i2c_addr, PI3USB30532_REG_CONTROL,
				 (reg & PI3USB30532_CTRL_MASK) |
				 PI3USB30532_CTRL_RSVD);
}

/* Flip polarity of USB mux */
static int pi3usb30532_flip_mux(int i2c_addr)
{
	uint8_t reg;

	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_CONTROL, &reg))
		return -1;

	if (reg & PI3USB30532_BIT_SWAP)
		reg &= ~(reg & PI3USB30532_BIT_SWAP);
	else
		reg |= PI3USB30532_BIT_SWAP;

	return pi3usb30532_write(i2c_addr, PI3USB30532_REG_CONTROL,
				 (reg & PI3USB30532_CTRL_MASK) |
				 PI3USB30532_CTRL_RSVD);
}

const struct usb_mux_driver pi3usb30532_usb_mux_driver = {
	.init = pi3usb30532_init,
	.get = pi3usb30532_get_mux,
	.set = pi3usb30532_set_mux,
	.flip = pi3usb30532_flip_mux,
};
