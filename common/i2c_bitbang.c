/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "i2c_bitbang.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

#define RETURN_ON_ERROR(expr) \
	if (expr) { \
		i2c_stop_cond(i2c_port); \
		CPRINTS("\x1b[1;31merror at line %d\x1b[m", __LINE__); \
		return EC_ERROR_UNKNOWN; \
	}

#define CHECK_ARBITRATION_LOST() \
	RETURN_ON_ERROR(gpio_get_level(i2c_port->sda) == 0)

int started = 0;

/* TODO: respect i2c_port->kbps setting */
static void i2c_delay(void)
{
	udelay(5);
}

static int clock_stretching(const struct i2c_port_t *i2c_port)
{
	int i;

	/* 4us * 9000 iterations ~= 36ms */
	for (i = 0; i < 9000; i++) {
		i2c_delay();
		if (gpio_get_level(i2c_port->scl))
			return 0;
	}
	return 1;
}

static int i2c_stop_cond(const struct i2c_port_t *i2c_port)
{
	int err = 0;

	if (!started)
		return 0;

	// set SDA to 0
	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	// Clock stretching
	err = clock_stretching(i2c_port);

	// Stop bit setup time, minimum 4us
	i2c_delay();

	// SCL is high, set SDA from 0 to 1
	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();

	started = 0;

	CHECK_ARBITRATION_LOST();

	return err;
}

static int i2c_start_cond(const struct i2c_port_t *i2c_port)
{
	if (started) {
		gpio_set_level(i2c_port->sda, 1);
		i2c_delay();
		gpio_set_level(i2c_port->scl, 1);
		RETURN_ON_ERROR(clock_stretching(i2c_port));
		i2c_delay();
	}

	CHECK_ARBITRATION_LOST();

	ASSERT(gpio_get_level(i2c_port->scl) == 1);

	// SCL is high, set SDA from 1 to 0.
	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();
	gpio_set_level(i2c_port->scl, 0);
	started = 1;

	return 0;
}

static int i2c_write_bit(const struct i2c_port_t *i2c_port, int bit)
{
	gpio_set_level(i2c_port->sda, !!bit);

	// SDA change propagation delay
	i2c_delay();

	// Set SCL high to indicate a new valid SDA value is available
	gpio_set_level(i2c_port->scl, 1);

	RETURN_ON_ERROR(clock_stretching(i2c_port));

	// Wait for SDA value to be read by slave, minimum of 4us for standard mode
	i2c_delay();

	// SCL is high, now data is valid
	// If SDA is high, check that nobody else is driving SDA
	if (bit)
		CHECK_ARBITRATION_LOST();

	// Clear the SCL to low in preparation for next change
	gpio_set_level(i2c_port->scl, 0);

	return 0;
}

static void i2c_read_bit(const struct i2c_port_t *i2c_port, int *bit)
{
	// Let the slave drive data
	gpio_set_level(i2c_port->sda, 1);

	// Wait for SDA value to be written by slave, minimum of 4us for standard mode
	i2c_delay();

	// Set SCL high to indicate a new valid SDA value is available
	gpio_set_level(i2c_port->scl, 1);

	clock_stretching(i2c_port);

	// Wait for SDA value to be written by slave, minimum of 4us for standard mode
	i2c_delay();

	// SCL is high, read out bit
	*bit = gpio_get_level(i2c_port->sda);

	// Set SCL low in preparation for next operation
	gpio_set_level(i2c_port->scl, 0);
}

static int i2c_write_byte(const struct i2c_port_t *i2c_port, uint8_t byte)
{
	int i, nack;

	for (i = 7; i >= 0; i--) {
		RETURN_ON_ERROR(i2c_write_bit(i2c_port, (byte & (1 << i)) != 0));
	}

	i2c_read_bit(i2c_port, &nack);

	return nack ? EC_ERROR_UNKNOWN : 0;
}

static int i2c_read_byte(const struct i2c_port_t *i2c_port, uint8_t *byte,
		int nack)
{
	int i;

	*byte = 0;
	for (i = 0; i < 8; i++) {
		int bit = 0;

		i2c_read_bit(i2c_port, &bit);
		*byte = (*byte << 1) | bit;
	}

	return i2c_write_bit(i2c_port, nack);
}

static int i2c_bitbang_xfer(const struct i2c_port_t *i2c_port,
		const uint16_t slave_addr_flags,
		const uint8_t *out, int out_size,
		uint8_t *in, int in_size, int flags)
{
	uint16_t addr_8bit = slave_addr_flags << 1;
	int i = 0;

	if (out_size) {
		RETURN_ON_ERROR(i2c_start_cond(i2c_port));

		RETURN_ON_ERROR(i2c_write_byte(i2c_port, addr_8bit));
		for (int i = 0; i < out_size; i++) {
			RETURN_ON_ERROR(i2c_write_byte(i2c_port, out[i]));
		}
	}

	if (in_size) {
		if (flags & I2C_XFER_START) {
			RETURN_ON_ERROR(i2c_start_cond(i2c_port));
			RETURN_ON_ERROR(i2c_write_byte(
						i2c_port, addr_8bit | 1));
		}

		for (i = 0; i < in_size; i++) {
			RETURN_ON_ERROR(i2c_read_byte(i2c_port, &in[i],
					(flags & I2C_XFER_STOP) && (i == in_size - 1)));
		}
	}

	if (flags & I2C_XFER_STOP)
		RETURN_ON_ERROR(i2c_stop_cond(i2c_port));

	return 0;
}

const struct i2c_drv bitbang_drv = {
	.xfer = &i2c_bitbang_xfer
};

