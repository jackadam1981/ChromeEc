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

#define T_TIMEOUT (35 * MSEC)  /* Detect clock low timeout, 25~35 ms */

static int started;

/* TODO: respect i2c_port->kbps setting */
static void i2c_delay(void)
{
	udelay(5);
}

/* Number of attempts to unwedge each pin. */
#define UNWEDGE_SCL_ATTEMPTS  10
#define UNWEDGE_SDA_ATTEMPTS  3

static void i2c_bitbang_unwedge(const struct i2c_port_t *i2c_port)
{
	int i, j;

	gpio_set_level(i2c_port->scl, 1);
	/*
	 * If clock is low, wait for a while in case of clock stretched
	 * by a slave.
	 */
	if (!gpio_get_level(i2c_port->scl)) {
		for (i = 0;; i++) {
			if (i >= UNWEDGE_SCL_ATTEMPTS) {
				/*
				 * If we get here, a slave is holding the clock
				 * low and there is nothing we can do.
				 */
				CPRINTS("I2C%d unwedge failed, "
					"SCL is held low", i2c_port->port);
				return;
			}
			i2c_delay();
			if (gpio_get_level(i2c_port->scl))
				break;
		}
	}

	if (gpio_get_level(i2c_port->sda))
		return;

	CPRINTS("I2C%d unwedge called with SDA held low", i2c_port->port);

	/* Keep trying to unwedge the SDA line until we run out of attempts. */
	for (i = 0; i < UNWEDGE_SDA_ATTEMPTS; i++) {
		/* Drive the clock high. */
		gpio_set_level(i2c_port->scl, 0);
		i2c_delay();

		/*
		 * Clock through the problem by clocking out 9 bits. If slave
		 * releases the SDA line, then we can stop clocking bits and
		 * send a STOP.
		 */
		for (j = 0; j < 9; j++) {
			if (gpio_get_level(i2c_port->sda))
				break;

			gpio_set_level(i2c_port->scl, 0);
			i2c_delay();
			gpio_set_level(i2c_port->scl, 1);
			i2c_delay();
		}

		/* Take control of SDA line and issue a STOP command. */
		gpio_set_level(i2c_port->sda, 0);
		i2c_delay();
		gpio_set_level(i2c_port->sda, 1);
		i2c_delay();

		/* Check if the bus is unwedged. */
		if (gpio_get_level(i2c_port->sda) &&
				gpio_get_level(i2c_port->scl))
			break;
	}

	if (!gpio_get_level(i2c_port->sda))
		CPRINTS("I2C%d unwedge failed, SDA still low", i2c_port->port);
	if (!gpio_get_level(i2c_port->scl))
		CPRINTS("I2C%d unwedge failed, SCL still low", i2c_port->port);
}

/* wait until given gpio became high, or timeout */
static int i2c_wait_gpio(enum gpio_signal signal, int timeout_us)
{
	int i;

	/*
	 * get_time()/timestamp_expired() seems to have performance impact here,
	 * So use raw loop instead.
	 */
	for (i = 0; i < timeout_us; i++) {
		if (gpio_get_level(signal))
			return EC_SUCCESS;
		udelay(1);
	}
	return EC_ERROR_TIMEOUT;
}

static int clock_stretching(const struct i2c_port_t *i2c_port)
{
	int err;

	i2c_delay();
	err = i2c_wait_gpio(i2c_port->scl, T_TIMEOUT);

	if (err)
		CPRINTS("bitbang CLK timeout");
	return err;
}

static int i2c_stop_cond(const struct i2c_port_t *i2c_port)
{
	int err;

	if (!started)
		return EC_SUCCESS;

	gpio_set_level(i2c_port->sda, 0);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	err = clock_stretching(i2c_port);
	if (err)
		return err;

	/*
	 * SMBus 3.0, 4.2.5
	 *
	 *  the recommendation is that if SMBDAT is still low tTIMEOUT,MAX after
	 *  SMBCLK has gone high at the end of a transaction the master should
	 *  hold SMBCLK low for at least tTIMEOUT,MAX in an attempt to reset the
	 *  SMBus interface of all of the devices on the bus.
	 */
	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();
	err = i2c_wait_gpio(i2c_port->sda, T_TIMEOUT);
	if (err)
		CPRINTS("bitbang DAT timeout");

	/* Delay tBUF(=4.7us) between STOP and next START */
	i2c_delay();
	return err;
}

static int i2c_start_cond(const struct i2c_port_t *i2c_port)
{
	int err;

	if (started) {
		gpio_set_level(i2c_port->sda, 1);
		i2c_delay();

		gpio_set_level(i2c_port->scl, 1);
		err = clock_stretching(i2c_port);
		if (err)
			return err;

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

	return EC_SUCCESS;
}

static int i2c_write_bit(const struct i2c_port_t *i2c_port, int bit)
{
	int err;

	gpio_set_level(i2c_port->sda, !!bit);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	err = clock_stretching(i2c_port);
	if (err)
		return err;

	if (bit && gpio_get_level(i2c_port->sda) == 0) {
		CPRINTS("%s: arbitration lost", __func__);
		started = 0;
		return EC_ERROR_UNKNOWN;
	}

	gpio_set_level(i2c_port->scl, 0);

	return EC_SUCCESS;
}

static int i2c_read_bit(const struct i2c_port_t *i2c_port, int *bit)
{
	int err;

	gpio_set_level(i2c_port->sda, 1);
	i2c_delay();

	gpio_set_level(i2c_port->scl, 1);
	err = clock_stretching(i2c_port);
	if (err)
		return err;
	*bit = gpio_get_level(i2c_port->sda);

	gpio_set_level(i2c_port->scl, 0);

	return EC_SUCCESS;
}

static int i2c_write_byte(const struct i2c_port_t *i2c_port, uint8_t byte)
{
	int i, nack, err;

	for (i = 7; i >= 0; i--) {
		err = i2c_write_bit(i2c_port, byte & (1 << i));
		if (err)
			return err;
	}

	err = i2c_read_bit(i2c_port, &nack);
	if (err)
		return err;

	if (nack)
		/* return EC_ERROR_BUSY to indicate i2c_xfer() to retry */
		return EC_ERROR_BUSY;

	return EC_SUCCESS;
}

static int i2c_read_byte(const struct i2c_port_t *i2c_port, uint8_t *byte,
		int nack)
{
	int i;

	*byte = 0;
	for (i = 0; i < 8; i++) {
		int bit = 0, err;

		err = i2c_read_bit(i2c_port, &bit);
		if (err)
			return err;
		*byte = (*byte << 1) | bit;
	}

	return i2c_write_bit(i2c_port, nack);
}

static int i2c_bitbang_xfer(const struct i2c_port_t *i2c_port,
		const uint16_t slave_addr_flags,
		const uint8_t *out, int out_size,
		uint8_t *in, int in_size, int flags)
{
	uint16_t addr_8bit = slave_addr_flags << 1, err = EC_SUCCESS;
	int i = 0;

	if (i2c_port->kbps != 100)
		CPRINTS("warning: bitbang driver only supports 100kbps");

	if (out_size) {
		if (flags & I2C_XFER_START) {
			err = i2c_start_cond(i2c_port);
			if (err)
				goto exit;
			err = i2c_write_byte(i2c_port, addr_8bit);
			if (err)
				goto exit;
		}

		for (i = 0; i < out_size; i++) {
			err = i2c_write_byte(i2c_port, out[i]);
			if (err)
				goto exit;
		}
	}

	if (in_size) {
		if (flags & I2C_XFER_START) {
			err = i2c_start_cond(i2c_port);
			if (err)
				goto exit;
			err = i2c_write_byte(i2c_port, addr_8bit | 1);
			if (err)
				goto exit;
		}

		for (i = 0; i < in_size; i++) {
			err = i2c_read_byte(i2c_port, &in[i],
				(flags & I2C_XFER_STOP) && (i == in_size - 1));
			if (err)
				goto exit;
		}
	}

	if (flags & I2C_XFER_STOP)
		err = i2c_stop_cond(i2c_port);

exit:
	/*
	 * SMBus 3.0, Note 3
	 * Devices participating in a transfer can abort the transfer in
	 * progress and release the bus when any single clock low interval
	 * exceeds the value of tTIMEOUT,MIN(=25ms).
	 * After the master in a transaction detects this condition, it must
	 * generate a stop condition within or after the current data byte in
	 * the transfer process.
	 */
	if (err) {
		if (i2c_stop_cond(i2c_port))
			/* slave holding the bus, try force unwedge */
			i2c_bitbang_unwedge(i2c_port);
		started = 0;
	}
	return err;
}

const struct i2c_drv bitbang_drv = {
	.xfer = &i2c_bitbang_xfer
};

#ifdef TEST_BUILD
int bitbang_start_cond(const struct i2c_port_t *i2c_port)
{
	return i2c_start_cond(i2c_port);
}

int bitbang_stop_cond(const struct i2c_port_t *i2c_port)
{
	return i2c_stop_cond(i2c_port);
}

int bitbang_write_byte(const struct i2c_port_t *i2c_port, uint8_t byte)
{
	return i2c_write_byte(i2c_port, byte);
}

void bitbang_set_started(int val)
{
	started = val;
}
#endif
