/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * ITE IT5205 Type-C USB alternate mode mux.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "usb_mux.h"
#include "usb_mux_it5205.h"
#include "util.h"

#define MUX_STATE_DP_USB_MASK (MUX_USB_ENABLED | MUX_DP_ENABLED)

static int it5205_read(int i2c_addr, uint8_t reg, int *val)
{
	return i2c_read8(I2C_PORT_USB_MUX, i2c_addr, reg, val);
}

static int it5205_write(int i2c_addr, uint8_t reg, uint8_t val)
{
	return i2c_write8(I2C_PORT_USB_MUX, i2c_addr, reg, val);
}

struct mux_chip_id_t {
	uint8_t chip_id;
	uint8_t reg;
};

static const struct mux_chip_id_t mux_chip_id_verify[] = {
	{ '5', IT5205_REG_CHIP_ID3},
	{ '2', IT5205_REG_CHIP_ID2},
	{ '0', IT5205_REG_CHIP_ID1},
	{ '5', IT5205_REG_CHIP_ID0},
};

static int it5205_init(int i2c_addr)
{
	int i, val, ret;

	/* bit[0]: mux power on, bit[7-1]: reserved. */
	ret = it5205_write(i2c_addr, IT5205_REG_MUXPDR, 0);
	if (ret)
		return ret;
	/*  Verify chip ID registers. */
	for (i = 0; i < ARRAY_SIZE(mux_chip_id_verify); i++) {
		ret = it5205_read(i2c_addr, mux_chip_id_verify[i].reg, &val);
		if (ret)
			return ret;

		if (val != mux_chip_id_verify[i].chip_id)
			return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

/* Writes control register to set switch mode */
static int it5205_set_mux(int i2c_addr, mux_state_t mux_state)
{
	uint8_t reg;

	switch (mux_state & MUX_STATE_DP_USB_MASK) {
	case MUX_USB_ENABLED:
		reg = IT5205_USB;
		break;
	case MUX_DP_ENABLED:
		reg = IT5205_DP;
		break;
	case MUX_STATE_DP_USB_MASK:
		reg = IT5205_DP_USB;
		break;
	default:
		reg = 0;
		break;
	}

	if (mux_state & MUX_POLARITY_INVERTED)
		reg |= IT5205_POLARITY_INVERTED;

	return it5205_write(i2c_addr, IT5205_REG_MUXCR, reg);
}

/* Reads control register and updates mux_state accordingly */
static int it5205_get_mux(int i2c_addr, mux_state_t *mux_state)
{
	int reg, ret;

	ret = it5205_read(i2c_addr, IT5205_REG_MUXCR, &reg);
	if (ret)
		return ret;

	switch (reg & IT5205_DP_USB_CTRL_MASK) {
	case IT5205_USB:
		*mux_state = MUX_USB_ENABLED;
		break;
	case IT5205_DP:
		*mux_state = MUX_DP_ENABLED;
		break;
	case IT5205_DP_USB:
		*mux_state = MUX_STATE_DP_USB_MASK;
		break;
	default:
		*mux_state = 0;
		break;
	}

	if (reg & IT5205_POLARITY_INVERTED)
		*mux_state |= MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

static int console_command_it5205(int argc, char **argv)
{
	int ret, addr, reg, val;
	char *e;

	if (argc < 4)
		return EC_ERROR_PARAM_COUNT;
	addr = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;
	reg = strtoi(argv[3], &e, 0);
	if (*e)
		return EC_ERROR_PARAM3;
	if (!strcasecmp(argv[1], "w")) {
		if (argc < 5)
			return EC_ERROR_PARAM_COUNT;
		val = strtoi(argv[4], &e, 0);
		if (*e)
			return EC_ERROR_PARAM4;
		ret = it5205_write(addr, reg, val);
		if (ret)
			return EC_ERROR_ACCESS_DENIED;
	}

	ret = it5205_read(addr, reg, &val);
	if (ret)
		return EC_ERROR_UNKNOWN;

	ccprintf("it5205 addr:%x reg:%xh value is %xh\n", addr, reg, val);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(it5205, console_command_it5205,
			"it5205 [r/w] [addr] [reg] | [val]",
			"Read or write a mux register");

static int console_command_it5205_get(int argc, char **argv)
{
	mux_state_t mux_state;
	int ret, addr;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	addr = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;
	ret = it5205_get_mux(addr, &mux_state);
	if (ret)
		return EC_ERROR_ACCESS_DENIED;

	if (mux_state)
		ccprintf("mux state : %s %s %s\n",
		(mux_state & MUX_USB_ENABLED)        ? "usb"  : "",
		(mux_state & MUX_DP_ENABLED)         ? "dp"   : "",
		(mux_state & MUX_POLARITY_INVERTED)  ? "flip" : "");
	else
		ccprintf("mux state : none\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(it5205_get, console_command_it5205_get,
			"it5205 [addr]", "");

static int console_command_it5205_set(int argc, char **argv)
{
	mux_state_t mux_state = 0;
	int ret, addr;
	char *e;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	addr = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (!strcasecmp(argv[2], "none"))
		mux_state = 0;
	else if (!strcasecmp(argv[2], "usb"))
		mux_state |= MUX_USB_ENABLED;
	else if (!strcasecmp(argv[2], "dp"))
		mux_state |= MUX_DP_ENABLED;
	else if (!strcasecmp(argv[2], "dock"))
		mux_state |= (MUX_DP_ENABLED | MUX_USB_ENABLED);
	else
		return EC_ERROR_PARAM2;

	if ((!strcasecmp(argv[3], "flip")) && (argc == 4))
		mux_state |= MUX_POLARITY_INVERTED;

	ret = it5205_set_mux(addr, mux_state);
	if (ret)
		return EC_ERROR_ACCESS_DENIED;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(it5205_set, console_command_it5205_set,
			"it5205 [addr] [none | usb | dp | dock] | [flip]",
			"");

const struct usb_mux_driver it5205_usb_mux_driver = {
	.init = it5205_init,
	.set = it5205_set_mux,
	.get = it5205_get_mux,
};
