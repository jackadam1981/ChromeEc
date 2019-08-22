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
	do { \
		int err = (expr); \
		if (err) { \
			CPRINTS("error %d at line %d", err, __LINE__); \
			return err; \
		} \
	} while (0)

#define CHECK_ARBITRATION_LOST() \

int started = 0;

/* TODO: respect i2c_port->kbps setting */
static void i2c_delay(void)
{
	udelay(5);
}

static void i2c_stop_cond(const struct i2c_port_t *i2c_port)
{
	int i;

	if (!started)
		return;

	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);

	/*
	 * TODO:
	 * SMBus 3.0, 4.2.5
	 *
	 *  the recommendation is that if SMBDAT is still low tTIMEOUT,MAX after
	 *  SMBCLK has gone high at the end of a transaction the master should
	 *  hold SMBCLK low for at least tTIMEOUT,MAX in an attempt to reset the
	 *  SMBus interface of all of the devices on the bus.
	 */
	for (i = 0; i < 7000; i++) {
		if (gpio_get_level(i2c_port->scl))
			break;
		i2c_delay();
	}
	i2c_delay();

	/* SCL is high, set SDA from 0 to 1 */
	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();

	started = 0;
}

static int clock_stretching(const struct i2c_port_t *i2c_port)
{
	int i;

	i2c_delay();
	/* 5us * 7000 iterations ~= 35ms */
	for (i = 0; i < 7000; i++) {
		if (gpio_get_level(i2c_port->scl))
			return 0;
		i2c_delay();
	}

	/*
	 * SMBus 3.0, Note 3
	 * Devices participating in a transfer can abort the transfer in
	 * progress and release the bus when any single clock low interval
	 * exceeds the value of tTIMEOUT,MIN(=25ms).
	 * After the master in a transaction detects this condition, it must
	 * generate a stop condition within or after the current data byte in
	 * the transfer process.
	 */
	i2c_stop_cond(i2c_port);
	CPRINTS("clock low timeout");

	return EC_ERROR_TIMEOUT;
}

static int i2c_start_cond(const struct i2c_port_t *i2c_port)
{
	if (started) {
		gpio_set_level(i2c_port->sda, 1);
		i2c_delay();

		gpio_set_level(i2c_port->scl, 1);
		RETURN_ON_ERROR(clock_stretching(i2c_port));
		i2c_delay();

		if (gpio_get_level(i2c_port->sda) == 0) {
			CPRINTS("%s: arbitration lost", __func__);
			started = 0;
			return EC_ERROR_UNKNOWN;
		}
	}

	/* check if bus is idle before starting */
	if (gpio_get_level(i2c_port->scl) == 0 ||
	    gpio_get_level(i2c_port->sda) == 0)
		return EC_ERROR_UNKNOWN;

	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 0);
	started = 1;

	return 0;
}

static int i2c_write_bit(const struct i2c_port_t *i2c_port, int bit)
{
	gpio_set_level(i2c_port->sda, !!bit);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	RETURN_ON_ERROR(clock_stretching(i2c_port));
	i2c_delay();

	if (bit && gpio_get_level(i2c_port->sda) == 0) {
		CPRINTS("%s: arbitration lost", __func__);
		started = 0;
		return EC_ERROR_UNKNOWN;
	}

	gpio_set_level(i2c_port->scl, 0);

	return 0;
}

static int i2c_read_bit(const struct i2c_port_t *i2c_port, int *bit)
{
	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	RETURN_ON_ERROR(clock_stretching(i2c_port));
	i2c_delay();
	*bit = gpio_get_level(i2c_port->sda);

	gpio_set_level(i2c_port->scl, 0);

	return 0;
}

static int i2c_write_byte(const struct i2c_port_t *i2c_port, uint8_t byte)
{
	int i, nack;

	for (i = 7; i >= 0; i--) {
		RETURN_ON_ERROR(i2c_write_bit(i2c_port, byte & (1 << i)));
	}

	RETURN_ON_ERROR(i2c_read_bit(i2c_port, &nack));

	return nack ? EC_ERROR_UNKNOWN : 0;
}

static int i2c_read_byte(const struct i2c_port_t *i2c_port, uint8_t *byte,
		int nack)
{
	int i;

	*byte = 0;
	for (i = 0; i < 8; i++) {
		int bit = 0;

		RETURN_ON_ERROR(i2c_read_bit(i2c_port, &bit));
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
		i2c_stop_cond(i2c_port);

	return 0;
}

const struct i2c_drv bitbang_drv = {
	.xfer = &i2c_bitbang_xfer
};

