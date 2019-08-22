/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio.h"
#include "i2c_bitbang.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

#define RETURN_ON_ERROR(expr) if (expr) { i2c_stop_cond(i2c_port); return 1; }

#define arbitration_lost() ASSERT(0)

int started = 0;

const struct i2c_drv bitbang_drv = {
	.xfer = &i2c_bitbang_xfer
};

/* TODO: respect i2c_port->kbps setting */
static void i2c_delay(void)
{
	udelay(4);
}

static int clock_stretching(const struct i2c_port_t *i2c_port) {
	int i;

	for (i = 0; i < 100; i++) {
		if (gpio_get_level(i2c_port->scl))
			return 0;
		i2c_delay();
	}
	return 1;
}

static void i2c_start_cond(const struct i2c_port_t *i2c_port) {
	if (started) {
		gpio_set_level(i2c_port->sda, 1);
		i2c_delay();
		gpio_set_level(i2c_port->scl, 1);
		clock_stretching(i2c_port);

		i2c_delay();
	}

	if (gpio_get_level(i2c_port->sda) == 0) {
		arbitration_lost();
	}

	ASSERT(gpio_get_level(i2c_port->scl) == 1);

	// SCL is high, set SDA from 1 to 0.
	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();
	gpio_set_level(i2c_port->scl, 0);
	started = 1;
}

static void i2c_stop_cond(const struct i2c_port_t *i2c_port) {
	// set SDA to 0
	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	// Clock stretching
	clock_stretching(i2c_port);

	// Stop bit setup time, minimum 4us
	i2c_delay();

	// SCL is high, set SDA from 0 to 1
	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();

	if (gpio_get_level(i2c_port->sda) == 0) {
		arbitration_lost();
	}

	started = 0;
}

static void i2c_write_bit(const struct i2c_port_t *i2c_port, int bit) {
	gpio_set_level(i2c_port->sda, !!bit);

	// SDA change propagation delay
	i2c_delay();

	// Set SCL high to indicate a new valid SDA value is available
	gpio_set_level(i2c_port->scl, 1);

	// Wait for SDA value to be read by slave, minimum of 4us for standard mode
	i2c_delay();

	clock_stretching(i2c_port);

	// SCL is high, now data is valid
	// If SDA is high, check that nobody else is driving SDA
	if (bit && (gpio_get_level(i2c_port->sda) == 0)) {
		arbitration_lost();
	}

	// Clear the SCL to low in preparation for next change
	gpio_set_level(i2c_port->scl, 0);
}

static int i2c_read_bit(const struct i2c_port_t *i2c_port) {
	int bit;

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
	bit = gpio_get_level(i2c_port->sda);

	// Set SCL low in preparation for next operation
	gpio_set_level(i2c_port->scl, 0);

	return bit;
}

static int i2c_write_byte(const struct i2c_port_t *i2c_port, uint8_t byte) {
	int i;

	for (i = 7; i >= 0; i--) {
		i2c_write_bit(i2c_port, (byte & (1 << i)) != 0);
	}

	return i2c_read_bit(i2c_port); /* nack */
}

static uint8_t i2c_read_byte(const struct i2c_port_t *i2c_port, int nack) {
	uint8_t byte = 0;
	int i;

	for (i = 0; i < 8; i++) {
		byte = (byte << 1) | i2c_read_bit(i2c_port);
	}

	i2c_write_bit(i2c_port, nack);

	return byte;
}

int i2c_bitbang_xfer(const struct i2c_port_t *i2c_port,
		const uint16_t slave_addr_flags,
		const uint8_t *out, int out_size,
		uint8_t *in, int in_size, int flags)
{
	uint16_t addr_8bit = slave_addr_flags << 1;
	int i;

	if (out_size) {
		i2c_start_cond(i2c_port);

		RETURN_ON_ERROR(i2c_write_byte(i2c_port, addr_8bit));
		for (int i = 0; i < out_size; i++) {
			RETURN_ON_ERROR(i2c_write_byte(i2c_port, out[i]));
		}
	}

	if (in_size) {
		if (flags & I2C_XFER_START) {
			i2c_start_cond(i2c_port);
			RETURN_ON_ERROR(i2c_write_byte(
						i2c_port, addr_8bit | 1));
		}

		for (i = 0; i < in_size; i++) {
			in[i] = i2c_read_byte(
					i2c_port,
					(flags & I2C_XFER_STOP) && (i == in_size - 1));
		}
	}
	if (flags & I2C_XFER_STOP)
		i2c_stop_cond(i2c_port);

	return 0;
}
