/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PI3USB9201 USB BC 1.2 Charger Detector driver definitions */

#include "i2c.h"

/* 8-bit I2C address */
#define PI3USB9201_I2C_ADDR_0 0xB8
#define PI3USB9201_I2C_ADDR_1 0xBA
#define PI3USB9201_I2C_ADDR_2 0xBC
#define PI3USB9201_I2C_ADDR_3 0xBE

#define PI3USB9201_REG_CTRL_1 0x0
#define PI3USB9201_REG_CTRL_2 0x1
#define PI3USB9201_REG_CLIENT_STS 0x2
#define PI3USB9201_REG_HOST_STS 0x3

/* Control_1 regiter bit definitions */
#define PI3USB9201_REG_CTRL_1_INT_MASK (1 << 0)
#define PI3USB9201_REG_CTRL_1_MODE_SHIFT 1
#define PI3USB9201_REG_CTRL_1_MODE_MASK ( \
		0x7 << PI3USB9201_REG_CTRL_1_MODE_SHIFT)

/* Control_2 regiter bit definitions */
#define PI3USB9201_REG_CTRL_2_AUTO_SW (1 << 1)
#define PI3USB9201_REG_CTRL_2_START_DET (1 << 3)

/* Host status register bit definitions */
#define PI3USB9201_REG_HOST_STS_BC12_DET (1 << 0)
#define PI3USB9201_REG_HOST_STS_DEV_PLUG (1 << 1)
#define PI3USB9201_REG_HOST_STS_DEV_UNPLUG (1 << 2)

struct pi3usb2901_config_t {
	const int i2c_port;
	const int i2c_addr;
};

enum pi3usb9201_mode {
	PI3USB9201_POWER_DOWN,
	PI3USB9201_SDP_HOST_MODE,
	PI3USB9201_DCP_HOST_MODE,
	PI3USB9201_CDP_HOST_MODE,
	PI3USB9201_CLIENT_MODE,
	PI3USB9201_RESERVED_1,
	PI3USB9201_RESERVED_2,
	PI3USB9201_USB_PATH_ON,
};

/* Configuration struct defined at board level */
extern const struct pi3usb2901_config_t pi3usb2901_bc12_chips[];

static inline int raw_read8(int port, int offset, int *value)
{
	return i2c_read8(pi3usb2901_bc12_chips[port].i2c_port,
			 pi3usb2901_bc12_chips[port].i2c_addr,
			 offset, value);
}

static inline int raw_write8(int port, int offset, int value)
{
	return i2c_write8(pi3usb2901_bc12_chips[port].i2c_port,
			  pi3usb2901_bc12_chips[port].i2c_addr,
			  offset, value);
}

static inline int pi3usb9201_raw(int port, int reg, int mask, int val)
{
	int rv;
	int reg_val;

	rv = raw_read8(port, reg, &reg_val);
	if (rv)
		return rv;

	reg_val &= ~mask;
	reg_val |= val;

	return raw_write8(port, reg, reg_val);
}

static inline int pi3usb9201_set_mode(int port, int desired_mode)
{
	return pi3usb9201_raw(port, PI3USB9201_REG_CTRL_1,
			      PI3USB9201_REG_CTRL_1_MODE_MASK,
			      desired_mode << PI3USB9201_REG_CTRL_1_MODE_SHIFT);
}

