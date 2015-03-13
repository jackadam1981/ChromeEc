/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "clock.h"
#include "common.h"
#include "registers.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "pmu.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_I2C, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

/*
 * i2c control modes
 */
enum {
	I2C_DISABLED = 0,
	I2C_BIT_BANGING = 1,
	I2C_INSTRUCTION_BASED = 2,
	I2C_RSVD = 3
};

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

/*
 * i2c configs
 *	clkdiv[I2C_SPEED_NUM_OPTIONS] = {10,  1,  1,  1};
 *	phasesteps_p0[I2C_SPEED_NUM_OPTIONS] = { 6, 10,  5,  2};
 *	phasesteps_p1[I2C_SPEED_NUM_OPTIONS] = { 6, 10,  5,  1};
 *	phasesteps_p2[I2C_SPEED_NUM_OPTIONS] = { 8, 13,  5,  2};
 *	phasesteps_p3[I2C_SPEED_NUM_OPTIONS] = { 6, 32, 11,  3};
 */
#define I2C_SPEED_CLKDIV 10
#define I2C_SPEED_P0 6
#define I2C_SPEED_P1 6
#define I2C_SPEED_P2 8
#define I2C_SPEED_P3 6

/*
 * Init the i2c peripheral
 * @note The speed setting assumes a APB clock of 26MHz
 * @param port i2c peripheral port address
 * @param speed i2c bus speed
 * @return 0 on successful init, error otherwise
 */
static void i2c_init(void)
{
	uint32_t port = 0;

	CPRINTS("I2C Init");

	pmu_clock_en(PERIPH_I2C);

	GREG32_I(I2C, port, CTRL_CLKDIV) = I2C_SPEED_CLKDIV;

	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P0, I2C_SPEED_P0);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P1, I2C_SPEED_P1);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P2, I2C_SPEED_P2);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P3, I2C_SPEED_P3);

	/* Disable Interrupt */
	GREG32_I(I2C, port, CTRL_INT_EN) = 0;

	GREG32_I(I2C, port, CTRL_MODE) = I2C_INSTRUCTION_BASED;
}
DECLARE_HOOK(HOOK_INIT, i2c_init, HOOK_PRIO_DEFAULT);


/*
 * Write to i2c slave with 16-bit address
 * @param dev_addr i2c device address
 * @param mem_addr Register / memory address
 * @param data data to write
 * @param len length in bytes
 * @return 0 on successful write, @ref I2C_STATUS_OFFSET error bits otherwise
 *
 * Write 0 up to 4 bytes, ("First Write Stage" or "FW Stage")
 *   FWBYTESCOUNT[5:3], 2^3 = 8
 * Read or write 0 up to 64 bytes ("RW Stage")
 *   RWBYTESCOUNT[18:12], 2^7 = 128
 */
#define I2C_MAX_XFER_LEN1 4
#define I2C_MAX_XFER_LEN2 64
static int i2c_write(uint32_t port, uint8_t dev_addr,
			const uint8_t *data1, int len1,
			const uint8_t *data2, int len2, int flags)
{
	uint32_t cmd;
	int cnt = 1000;
	int i, j;
	volatile uint32_t *rw = GREG32_ADDR_I(I2C, port, RW0);
	int started = (flags & I2C_XFER_START) ? 0 : 1;

	if (len1 > I2C_MAX_XFER_LEN1)
		return -len1;

	if (len2 > I2C_MAX_XFER_LEN2)
		return -len2;

	cmd  = started << 1;   /* START = 1 */
	cmd |= 1 << 2;         /* FWDEVADDR = 1 */
	cmd |= len1 << 3;      /* FWBYTESCOUNT = len1 */
	cmd |= len2 << 12;     /* RWBYTESCOUNT = len2 */
	cmd |= 1 << 23;        /* FINALSTOP = 1 */
	cmd |= (dev_addr & 0x3F) << 25; /* DEVADDRVAL = dev_addr */

	/* Address: FIX_ME byte order ? */
	GREG32_I(I2C, port, FW) = 0;
	for (i = 0; i < len1; i++)
		GREG32_I(I2C, port, FW) |= (data1[i] << (8*i));

	for (i = 0; i < len2; j++) {
		j = i & 0x3;
		if (0 == j)
			rw[i >> 2] = 0;
		rw[i >> 2] |=  (data2[i] << (8*j));
	}

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
 * @param len length in bytes
 * @return 0 on successful read, @ref I2C_STATUS_OFFSET error bits otherwise
 */
static int i2c_read(uint32_t port, uint8_t dev_addr,
			const uint8_t *data1, int len1,
			uint8_t *data2, int len2, int flags)
{
	int cnt = 1000;
	uint32_t cmd;
	int i;
	volatile uint32_t *rw = GREG32_ADDR_I(I2C, port, RW0);
	int started = (flags & I2C_XFER_START) ? 0 : 1;

	if (len1 > I2C_MAX_XFER_LEN1)
		return -len1;

	if (len2 > I2C_MAX_XFER_LEN2)
		return -len2;

	cmd  = started << 1;   /* START = 1 */
	cmd |= 1 << 2;         /* FWDEVADDR = 1 */
	cmd |= len1 << 3;      /* FWBYTESCOUNT = 2 */
	cmd |= 1 << 9;         /* REPEATEDSTART = 1 */
	cmd |= 1 << 10;        /* RWDEVADDR = 1 */
	cmd |= 1 << 11;        /* RWDEVADDR_RWB = 1 */
	cmd |= len2 << 12;     /* RWBYTESCOUNT = 1 */
	cmd |= 1 << 19;        /* FINALNA = 1 */
	cmd |= 1 << 23;        /* FINALSTOP = 1 */
	cmd |= dev_addr << 25; /* DEVADDRVAL = dev_addr */

	/* Address: FIX_ME byte order ? */
	GREG32_I(I2C, port, FW) = 0;
	for (i = 0; i < len1; i++)
		GREG32_I(I2C, port, FW) |= (data1[i] << (8*i));

	/* Start transaction */
	GREG32_I(I2C, port, INST) = cmd;

	/* Wait for completion interrupt */
	while ((--cnt > 0) &&
		(GREG32_I(I2C, port, STATUS) & GC_I2C_STATUS_INTB_MASK))
		usleep(100);

	for (i = 0; i < len2; i++)
		data2[i] = rw[i>>2] >> (8 * (i & 0x3));

	return GREG32_I(I2C, port, STATUS);
}

int i2c_is_busy(int port)
{
	return GREAD_FIELD_I(I2C, port, STATUS, FWBYTESCOUNT) +
		GREAD_FIELD_I(I2C, port, STATUS, RWBYTESCOUNT);
}

void i2c_set_timeout(int port, uint32_t timeout)
{
	uint32_t clock_stretch = 1 << GC_I2C_CTRL_CS_EN_LSB;
	clock_stretch |= 1 << GC_I2C_CTRL_CS_TIMEOUTEN_LSB;
	clock_stretch |= (timeout << GC_I2C_CTRL_CS_TIMEOUTVAL_LSB) &
				GC_I2C_CTRL_CS_TIMEOUTVAL_MASK;
	GREG32_I(I2C, port, CTRL_CS) = clock_stretch;
}

int i2c_raw_get_scl(int port)
{
	enum gpio_signal g;
	/* int val = GREAD_FIELD_I(I2C, port, READVAL, SCL); */

	/* If no SCL pin defined for this port, then return 1 to appear idle. */
	if (get_scl_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

int i2c_raw_get_sda(int port)
{
	enum gpio_signal g;
	/* int val = GREAD_FIELD_I(I2C, port, READVAL, SDA); */

	/* If no SDA pin defined for this port, then return 1 to appear idle. */
	if (get_sda_from_i2c_port(port, &g) != EC_SUCCESS)
		return 1;

	return gpio_get_level(g);
}

int i2c_get_line_levels(int port)
{
	return (i2c_raw_get_sda(port) ? I2C_LINE_SDA_HIGH : 0) |
		(i2c_raw_get_scl(port) ? I2C_LINE_SCL_HIGH : 0);
}

static int wait_idle(int port)
{
	uint8_t sts, busy;

	busy = i2c_is_busy(port);
	while (busy) {
		usleep(100);
		busy = i2c_is_busy(port);

		sts = GREG32_I(I2C, port, STATUS);
		sts &= ~(GC_I2C_STATUS_FWBYTESCOUNT_MASK);
		sts &= ~(GC_I2C_STATUS_RWBYTESCOUNT_MASK);
		sts &= ~(GC_I2C_STATUS_INTB_MASK);
		if (sts)
			return EC_ERROR_UNKNOWN;
	}
	sts = GREG32_I(I2C, port, STATUS);
	if (sts)
		return EC_ERROR_UNKNOWN;
	return EC_SUCCESS;
}

int i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
		uint8_t *in, int in_size, int flags)
{
	int rc, len1, len2;

	if (out_size < 0)
		return EC_ERROR_UNKNOWN;

	if (in_size < 0)
		return EC_ERROR_UNKNOWN;

	if (out_size == 0 && in_size == 0)
		return EC_SUCCESS;

	rc = wait_idle(port);
	if (rc)
		return rc;

	if (out_size > I2C_MAX_XFER_LEN1) {

		if (in_size > 0) /* TBD */
			return EC_ERROR_UNKNOWN;

		len1 = 1;
		len2 = out_size - len1;
		rc = i2c_write(port, slave_addr, out, len1,
				&out[len1], len2, flags);
	} else /* out_size  >= 0 */
		rc = i2c_read(port, slave_addr, out, out_size,
				in, in_size, flags);

	return rc;
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

static void i2cs_init(void)
{
	int i;
	uint32_t port = 0;
	volatile uint32_t *rbuf = GREG32_ADDR_I(I2CS, port, READ_BUFFER0);

	CPRINTS("I2CS Init");

	pmu_clock_en(PERIPH_I2CS);

	GREG32_I(I2CS, port, SLAVE_DEVADDRVAL) = GC_I2CS_ADDRESS;

	/* Disable Interrupt */
	GREG32_I(I2CS, port, INT_ENABLE) = 0;

	/* Init I2CS Read Buffer */
	for (i = 0; i < 16; i++)
		rbuf[i] = 0xface0000 + i;
}
DECLARE_HOOK(HOOK_INIT, i2cs_init, HOOK_PRIO_DEFAULT);
