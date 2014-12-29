/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "atomic.h"
#include "clock.h"
#include "common.h"
#include "registers.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/*
 * i2c speed options
 */
enum {
	I2C_SPEED_STANDARD = 0,         /* 100 KHz */
	I2C_SPEED_FAST = 1,             /* 400 KHz */
	I2C_SPEED_FAST_MODE_PLUS = 2,   /*   1 MHz */
	I2C_SPEED_HIGH_SPEED = 3,       /* 3.4 MHz */
	I2C_SPEED_NUM_OPTIONS = 4
};

static const uint32_t clkdiv[I2C_SPEED_NUM_OPTIONS] = {10,  1,  1,  1};
static const uint32_t phasesteps_p0[I2C_SPEED_NUM_OPTIONS] = { 6, 10,  5,  2};
static const uint32_t phasesteps_p1[I2C_SPEED_NUM_OPTIONS] = { 6, 10,  5,  1};
static const uint32_t phasesteps_p2[I2C_SPEED_NUM_OPTIONS] = { 8, 13,  5,  2};
static const uint32_t phasesteps_p3[I2C_SPEED_NUM_OPTIONS] = { 6, 32, 11,  3};

/*
 * Init the i2c peripheral
 * Peripheral clock must already be enabled
 * @note The speed setting assumes a APB clock of 26MHz
 * @param port i2c peripheral port address
 * @param speed i2c bus speed
 * @return 0 on successful init, error otherwise
 */
static void i2c_init(void)
{
	uint32_t port = 0;
	uint32_t speed = I2C_SPEED_STANDARD;
	/* Set fast clock div */
	GREG32_I(I2C, port, CTRL_CLKDIV) = clkdiv[speed];

	/* Set phase steps */
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P0, phasesteps_p0[speed]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P1, phasesteps_p1[speed]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P2, phasesteps_p2[speed]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P3, phasesteps_p3[speed]);

	/* Set portruction portd I2C mode */
	GREG32_I(I2C, port, CTRL_MODE) = 0x2;
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_DEFAULT);


/*
 * Write to i2c slave with 16-bit address
 * @param dev_addr i2c device address
 * @param mem_addr Register / memory address
 * @param data Byte to write
 * @return 0 on successful write, @ref I2C_STATUS_OFFSET error bits otherwise
 */
int i2c_write_a16(uint32_t port, uint8_t dev_addr,
			uint16_t mem_addr, uint8_t data)
{
	uint32_t cmd;
	int cnt = 1000;

	cmd  = 1 << 1;         /* START = 1 */
	cmd |= 1 << 2;         /* FWDEVADDR = 1 */
	cmd |= 2 << 3;         /* FWBYTESCOUNT = 2 */
	cmd |= 1 << 12;        /* RWBYTESCOUNT = 1 */
	cmd |= 1 << 23;        /* FINALSTOP = 1 */
	cmd |= dev_addr << 25; /* DEVADDRVAL = dev_addr */

	/* Address */
	GREG32_I(I2C, port, FW) = ((mem_addr & 0xff) << 8) | (mem_addr >> 8);

	/* Write value */
	GREG32_I(I2C, port, RW0) = data;

	/* Start transaction */
	GREG32_I(I2C, port, INST) = cmd;

	/* Wait for completion interrupt */
	while ((--cnt > 0) &&
		(GREG32_I(I2C, port, STATUS) & GC_I2C_STATUS_INTB_MASK))
		usleep(100);

	return GREG32_I(I2C, port, STATUS);
}

/*
 * Read from i2c slave with 16-bit address
 * @param dev_addr i2c device address
 * @param mem_addr Register / memory address
 * @param data Pointer to byte to store read value
 * @return 0 on successful read, @ref I2C_STATUS_OFFSET error bits otherwise
 */
int i2c_read_a16(uint32_t port, uint8_t dev_addr,
			uint16_t mem_addr, uint8_t *data)
{
	int cnt = 1000;
	uint32_t cmd;
	cmd  = 1 << 1;         /* START = 1 */
	cmd |= 1 << 2;         /* FWDEVADDR = 1 */
	cmd |= 2 << 3;         /* FWBYTESCOUNT = 2 */
	cmd |= 1 << 9;         /* REPEATEDSTART = 1 */
	cmd |= 1 << 10;        /* RWDEVADDR = 1 */
	cmd |= 1 << 11;        /* RWDEVADDR_RWB = 1 */
	cmd |= 1 << 12;        /* RWBYTESCOUNT = 1 */
	cmd |= 1 << 19;        /* FINALNA = 1 */
	cmd |= 1 << 23;        /* FINALSTOP = 1 */
	cmd |= dev_addr << 25; /* DEVADDRVAL = dev_addr */

	/* Address */
	GREG32_I(I2C, port, FW) =
		((mem_addr & 0xff) << 8) | (mem_addr >> 8);

	/* Start transaction */
	GREG32_I(I2C, port, INST) = cmd;

	/* Wait for completion interrupt */
	while ((--cnt > 0) &&
		(GREG32_I(I2C, port, STATUS) & GC_I2C_STATUS_INTB_MASK))
		usleep(100);

	*data = GREG32_I(I2C, port, RW0);

	return GREG32_I(I2C, port, STATUS);
}

int i2c_is_busy(int port)
{
	/* TBD */
	return 0;
}

void i2c_set_timeout(int port, uint32_t timeout)
{
	/* TBD */
}

int i2c_raw_get_scl(int port)
{
	enum gpio_signal g;

	/* If no SCL pin defined for this port, then return 1 to appear idle. */
	if (get_scl_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

int i2c_raw_get_sda(int port)
{
	enum gpio_signal g;

	/* If no SDA pin defined for this port, then return 1 to appear idle. */
	if (get_sda_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

int i2c_get_line_levels(int port)
{
	/* TBD: need by i2c_xfer */
	return 0;
}

int i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
		uint8_t *in, int in_size, int flags)
{
	/* TBD */
	return 0;
}

int i2c_read_string(int port, int slave_addr, int offset, uint8_t *data,
		    int len)
{
	int rv;
	uint8_t reg, block_length = 0;

	i2c_lock(port, 1);

	reg = offset;
	/*
	 * Send device reg space offset, and read back block length.  Keep this
	 * session open without a stop.
	 */
	rv = i2c_xfer(port, slave_addr, &reg, 1, &block_length, 1,
		      I2C_XFER_START);
	if (rv)
		goto exit;

	if (len && block_length > (len - 1))
		block_length = len - 1;

	rv = i2c_xfer(port, slave_addr, 0, 0, data, block_length,
		      I2C_XFER_STOP);
	data[block_length] = 0;

exit:
	i2c_lock(port, 0);
	return rv;
}

/**
 * Handle an interrupt on the specified port.
 *
 * @param port          I2C port generating interrupt
 */
static void handle_interrupt(int port)
{
	/* TBD */
}

void i2c0_interrupt(void) { handle_interrupt(0); }
void i2c1_interrupt(void) { handle_interrupt(1); }

DECLARE_IRQ(GC_IRQNUM_I2C0_I2CINT, i2c0_interrupt, 2);
DECLARE_IRQ(GC_IRQNUM_I2C1_I2CINT, i2c1_interrupt, 2);
