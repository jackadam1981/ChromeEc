/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Cypress CCGXXF I/O Port expander (built inside PD chip) driver source
 */

#include "i2c.h"
#include "ioexpander.h"
#include "ccgxxf.h"

static inline int ccgxxf_read(int ioex, uint16_t reg, uint16_t *data)
{
	return i2c_read16(ioex_config[ioex].i2c_host_port,
			ioex_config[ioex].i2c_addr_flags, reg, data);
}

static inline int ccgxxf_write(int ioex, uint16_t reg, uint16_t data)
{
	return i2c_write16(ioex_config[ioex].i2c_host_port,
			ioex_config[ioex].i2c_addr_flags, reg, data);
}

static int ccgxxf_set_flags_by_mask(int ioex, int port, int mask, int flags)
{
	uint16_t data;

	data = (port << CCGXXF_GPIO_PORT_NUM_SHIFT) | mask;

	/* TODO: based on flags, manipulate data for mode reg */

	return ccgxxf_write(ioex, CCGXXF_REG_GPIO_MODE, data);
}

static int ccgxxf_get_flags_by_mask(int ioex, int port, int mask, int *flags)
{
	/*
	 * CCGXXF doesn't have register to report the flags by mask.
	 * If needed, store the flags locally and return them.
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

static int ccgxxf_get_level(int ioex, int port, int mask, int *val)
{
	int rv;
	uint16_t data = CCGXXF_GPIO_CTRL_READ_PIN |
		((port << CCGXXF_GPIO_PORT_NUM_SHIFT) | mask);

	rv = ccgxxf_write(ioex, CCGXXF_REG_GPIO_CONTROL, data);
	if (rv)
		return rv;

	rv = ccgxxf_read(ioex, CCGXXF_REG_GPIO_RESPONSE, &data);
	if (!rv)
		*val = data & CCGXXF_GPIO_RESP_PIN_STATE_MASK;

	return rv;
}

static int ccgxxf_set_level(int ioex, int port, int mask, int val)
{
	uint16_t data =
		(val ? CCGXXF_GPIO_CTRL_SET_HIGH : CCGXXF_GPIO_CTRL_SET_LOW) |
		((port << CCGXXF_GPIO_PORT_NUM_SHIFT) | mask);

	return ccgxxf_write(ioex, CCGXXF_REG_GPIO_CONTROL, data);
}

static int ccgxxf_enable_interrupt(int ioex, int port, int mask, int enable)
{
	/* TODO: Add code */
	return EC_SUCCESS;
}

int ccgxxf_init(int ioex)
{
	return EC_SUCCESS;
}

const struct ioexpander_drv ccgxxf_ioexpander_drv = {
	.init			= &ccgxxf_init,
	.get_level		= &ccgxxf_get_level,
	.set_level		= &ccgxxf_set_level,
	.get_flags_by_mask	= &ccgxxf_get_flags_by_mask,
	.set_flags_by_mask	= &ccgxxf_set_flags_by_mask,
	.enable_interrupt	= &ccgxxf_enable_interrupt,
};
