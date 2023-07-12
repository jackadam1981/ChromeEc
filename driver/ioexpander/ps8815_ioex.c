/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "ioexpander.h"
#include "system.h"
#include "ps8815_ioex.h"

#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ##args)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ##args)

static int ps8815_ioex_write_byte(int ioex, int port, int reg, uint8_t val)
{
	const struct ioexpander_config_t *ioex_p = &ioex_config[ioex];
	const int reg_addr = reg + port;

	if (port > PS8815_IOEX_PORT1)
		CPRINTF("PS8815 ioex_write invalid port %d\n", port);
		return EC_RES_INVALID_PARAM;
	}

	return i2c_write8(ioex_p->i2c_host_port, ioex_p->i2c_addr_flags,
			  reg_addr, val);
}

static int ps8815_ioex_read_byte(int ioex, int port, int reg, int *val)
{
	const struct ioexpander_config_t *ioex_p = &ioex_config[ioex];
	const int reg_addr = reg + port;

	if (port > PS8815_IOEX_PORT1)
		CPRINTF("PS8815 ioex_read invalid port %d\n", port);
		return EC_RES_INVALID_PARAM;
	}

	return i2c_read8(ioex_p->i2c_host_port, ioex_p->i2c_addr_flags,
			 reg_addr, val);
}

/* Initialize IO expander chip/driver */
static int ps8815_ioex_init(int ioex)
{
	int ret;

	/* Set default to input for both ports */
	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT0,
				PS8815_IO_REG_DIRECTION,
				PS8815_IOEX_PORT0_MASK)
	if (ret)
		return ret;

	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT1,
				PS8815_IO_REG_DIRECTION,
				PS8815_IOEX_PORT1_MASK)
	if (ret)
		return ret;

	/* Enable GPIO control for both ports */
	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT0,
				PS8815_IO_REG_OUT_SEL,
				PS8815_IOEX_PORT0_MASK)
	if (ret)
		return ret;
	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT1,
				PS8815_IO_REG_OUT_SEL,
				PS8815_IOEX_PORT1_MASK)
	if (ret)
		return ret;
	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT0,
				PS8815_IO_REG_OEB_SEL,
				S8815_IOEX_PORT0_MASK)
	if (ret)
		return ret;
	ret = ps8815_ioex_write_byte(ioex, PS8815_IOEX_PORT1,
				PS8815_IO_REG_OEB_SEL,
				PS8815_IOEX_PORT1_MASK)
	if (ret)
		return ret;

	CPRINTF("PS8815 ioex successully initialized!\n");
	return EC_SUCCESS;
}

/* Get the current level of the IOEX pin */
static int ps8815_ioex_get_level(int ioex, int port, int mask, int *val)
{
	int buf = 0;
	int ret;

	ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_LEVEL, &buf);
	*val = !!(buf & mask);

	return ret;
}

/* Set the level of the IOEX pin */
static int ps8815_ioex_set_level(int ioex, int port, int mask, int val)
{
	int ret;
	int v;

	ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_LEVEL, &v);
	if (ret)
		return ret;

	if (val)
		v |= mask;
	else
		v &= ~mask;

	return ps8815_ioex_write_byte(ioex, port, PS8815_IO_REG_LEVEL, v);
}

/* Get flags for the IOEX pin */
static int ps8815io_get_flags_by_mask(int ioex, int port, int mask, int *flags)
{
	int ret;
	int v;

	ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_DIRECTION, &v);
	if (ret)
		return ret;

	*flags = 0;
	if (v & mask) {
		*flags |= GPIO_OUTPUT;

		ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_LEVEL,
			&v);
		if (ret)
			return ret;

		if (v & mask)
			*flags |= GPIO_HIGH;
		else
			*flags |= GPIO_LOW;
	} else {
		*flags |= GPIO_INPUT;
	}

	return EC_SUCCESS;
}

/* Set flags for the IOEX pin */
static int ps8815io_set_flags_by_mask(int ioex, int port, int mask, int flags)
{
	int ret;
	int v;

	/* Output value */
	if (flags & GPIO_OUTPUT) {
		ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_DIRECTION,
			&v);
		if (ret)
			return ret;

		if (flags & GPIO_HIGH)
			v &= ~mask;
		else if (flags & GPIO_LOW)
			v |= mask;
		else
			return EC_ERROR_INVAL;

		ret = ps8815_ioex_write_byte(ioex, port,
			PS8815_IO_REG_DIRECTION, v);
		if (ret)
			return ret;
	}

	/* Configuration */
	ret = ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_LEVEL, &v);
	if (ret)
		return ret;

	if (flags & GPIO_INPUT)
		v |= mask;
	else if (flags & GPIO_OUTPUT)
		v &= ~mask;
	else
		return EC_ERROR_INVAL;

	ret = ps8815_ioex_write_byte(ioex, port, PS8815_IO_REG_LEVEL, v);
	if (ret)
		return ret;

	return EC_SUCCESS;
}

#ifdef CONFIG_IO_EXPANDER_SUPPORT_GET_PORT

/* Read levels for whole IO expander port */
static int ps8815_ioex_get_port(int ioex, int port, int *val)
{
	return ps8815_ioex_read_byte(ioex, port, PS8815_IO_REG_LEVEL, val);
}

#endif

/* Driver structure */
const struct ioexpander_drv ps8815_ioexpander_drv = {
	.init = ps8815_ioex_init,
	.get_level = ps8815_ioex_get_level,
	.set_level = ps8815_ioex_set_level,
	.get_flags_by_mask = ps8815io_get_flags_by_mask,
	.set_flags_by_mask = ps8815io_set_flags_by_mask,
	.enable_interrupt = NULL,
#ifdef CONFIG_IO_EXPANDER_SUPPORT_GET_PORT
	.get_port = ps8815_ioex_get_port,
#endif
};
