/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA(L)6408 I/O expander
 */

#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "ioexpander.h"
#include "ioexpander_pca6408.h"

static int pca6408_read(int ioex, int reg, int *data)
{
	int rv;
	struct ioexpander_config_t *ioex_p = &ioex_config[ioex];
	
	rv = i2c_read8(ioex_p->i2c_host_port, ioex_p->i2c_slave_addr,
			reg, data);

	return rv;
}

static int pca6408_write(int ioex, int reg, int data)
{
	int rv;
	struct ioexpander_config_t *ioex_p = &ioex_config[ioex];

	rv = i2c_write8(ioex_p->i2c_host_port, ioex_p->i2c_slave_addr,
			reg, data);

	return rv;
}

static int pca6408_ioex_check_is_valid(int port, int mask)
{
	if (port != 0)
		return EC_ERROR_INVAL;

	if (mask & ~PCA6408_VALID_GPIO_MASK) {
		CPRINTF("GPIO%02d is not support in PCA6408\n",
						_fls(mask));
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int pca6408_ioex_init(int ioex)
{
	/* If interrupt need latch, here should do the settings */
	return EC_SUCCESS;
}

static int pca6408_ioex_get_level(int ioex, int port, int mask, int *val)
{
	int rv;

	rv = pca6408_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv = pca6408_read(ioex, PCA6408_INPUT, val);
	if (rv != EC_SUCCESS)
		return rv;

	*val = !!(*val & mask);

	return EC_SUCCESS;
}

static int pca6408_ioex_set_level(int ioex, int port, int mask, int value)
{
	int rv, val;

	rv = pca6408_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv = pca6408_read(ioex, PCA6408_OUTPUT, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (value)
		val |= mask;
	else
		val &= ~mask;

	return pca6408_write(ioex, PCA6408_OUTPUT, val);
}

static int pca6408_ioex_get_flags_by_mask(int ioex, int port, int mask, int *flags)
{
	int rv, val;

	rv = pca6408_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	*flags = GPIO_FLAG_NONE;

	rv = pca6408_read(ioex, PCA6408_CONFIG, &val)
	if (rv != EC_SUCCESS)
		return rv;

	if (val & mask)
		*flags |= GPIO_INPUT;
	else
		*flags |= GPIO_OUTPUT;

	rv = pca6408_read(ioex, PCA6408_INPUT, &val)
	if (rv != EC_SUCCESS)
		return rv;

	if (val & mask)
		*flags |= GPIO_HIGH;
	else
		*flags |= GPIO_LOW;
	
	rv = pca6408_read(ioex, PCA6408_OUT_CONFIG, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (val & PCA_OUT_CONFIG_OPEN_DRAIN)
		*flags |= GPIO_OPEN_DRAIN;
	
	rv = pca6408_read(ioex, PCA6408_PULL_ENABLE, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (val & mask) {
		rv = pca6408_read(ioex, PCA6408_PULL_UP_DOWN, &val);
		if (rv != EC_SUCCESS)
			return rv;

		if (val & mask)
			*flags |= GPIO_PULL_UP;
		else
			*flags |= GPIO_PULL_DOWN;
	}

	rv = pca6408_read(ioex, PCA6408_INT_MASK, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if ((!!(val & mask) == 0) && ((*flags) & GPIO_INPUT))
		*flags |= GPIO_INT_BOTH;
	
	return rv;
}

static int pca6408_ioex_set_flags_by_mask(int ioex, int port, int mask,
					int flags)
{
	int rv, val;

	rv = pca6408_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;
	
	if (((flags & GPIO_INT_BOTH) == GPIO_INT_RISING) ||
		((flags & GPIO_INT_BOTH) == GPIO_INT_FALLING))
		CPRINTF("PCA6408 only support GPIO_INT_BOTH.\n");
		return EC_ERROR_INVAL;
	}


	if ((flags & (GPIO_INT_F_RISING | GPIO_INT_F_FALLING)) &&
		!(flags & GPIO_INPUT))
		CPRINTF("Interrupt pin must be GPIO_INPUT.\n");
		return EC_ERROR_INVAL;
	}

	/* All output gpios share GPIO_OPEN_DRAIN, should be consistent */
	if (flags & GPIO_OPEN_DRAIN)
		val = PCA6408_OUT_OPEN_DRAIN;
	else
		val = 0;

	rv = pca6408_write(ioex, PCA6408_OUT_CONFIG, val);
	if (rv != EC_SUCCESS)
		return rv;

	rv = pca6408_read(ioex, PCA6408_CONFIG, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (flags & GPIO_INPUT)
		val |= mask;
	if (flags & GPIO_OUTPUT)
		val &= ~mask;
		
	rv = pca6408_write(ioex, PCA6408_CONFIG, val);
	if(rv != EC_SUCCESS)
		return rv;
	
	if (flags & GPIO_OUTPUT) {
		rv = pca6408_read(ioex, PCA6408_OUTPUT, &val);
		if (rv != EC_SUCCESS)
			return rv;

		if (flags & GPIO_HIGH)
			val |= mask;
		else
			val &= ~mask;

		rv = pca6408_write(ioex, PCA6408_OUTPUT, &val);
		if (rv != EC_SUCCESS)
			return rv;
	}

	if (!(flags & (GPIO_PULL_UP | GPIO_PULL_DOWN))) {
		rv = pca6408_read(ioex, PCA6408_PULL_ENABLE, &val);
		if (rv != EC_SUCCESS)
			return rv;

		val &= ~mask;

		rv = pca6408_write(ioex, PCA6408_PULL_ENABLE, val);
		if (rv != EC_SUCCESS)
			return rv;
	} else {
		rv = pca6408_read(ioex, PCA6408_PULL_ENABLE, &val);
		if (rv != EC_SUCCESS)
			return rv;

		val |= mask;

		rv = pca6408_write(ioex, PCA6408_PULL_ENABLE, val);
		if (rv != EC_SUCCESS)
			return rv;

		rv = pca6408_read(ioex, PCA6408_PULL_UP_DOWN, &val);
		if (rv != EC_SUCCESS)
			return rv;

		if (flags & GPIO_PULL_UP)
			val |= mask;
		else if (flags & GPIO_PULL_DOWN)
			val &= ~mask;

		rv = pca6408_write(ioex, PCA6408_PULL_UP_DOWN, val);
		if (rv != EC_SUCCESS)
			return rv;
	}

	return rv;
}

static int pca6408_ioex_enable_interrupt(int ioex, int port, int mask,
					int enable)
{
	int rv, val;
	
	rv = pca6408_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;
	
	/* Clear the pending bit */
	/*
	rv = pca6408_read(ioex, PCA6408_INT_STATUS, &val);
	if (rv != EC_SUCCESS)
		return rv;
	*/

	rv = pca6408_read(ioex, PCA6408_INT_MASK, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (enable)
		val |= mask;
	else
		val &= ~mask;
	
	return pca6408_write(ioex, PCA6408_INT_MASK, val);
}


const struct ioexpander_drv pca6408_ioexpander_drv = {
	.init			= &pca6408_ioex_init,
	.get_level		= &pca6408_ioex_get_level,
	.set_level		= &pca6408_ioex_set_level,
	.get_flags_by_mask	= &pca6408_ioex_get_flags_by_mask,
	.set_flags_by_mask	= &pca6408_ioex_set_flags_by_mask,
	.enable_interrupt	= &pca6408_ioex_enable_interrupt,
};
