/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is a driver for the I2C Master controller (i2cm) of the g chip.
 *
 * The g chip i2cm module supports 3 modes of operation, disabled, bit-banging,
 * and instruction based. These modes are selected via the I2C_CTRL
 * register. Selecting disabled mode can be used as a soft reset where the i2cm
 * hw state machine is reset, but the register values remain unchanged. In
 * bit-banging mode the signals SDA/SCL are controlled by the lower two bits of
 * the INST register. I2C_INST[1:0] = SCL|SDA. In this mode the value of SDA is
 * read every clock cycle.
 *
 * The main operation mode is instruction mode. A 32 bit instruction register
 * (I2C_INST) is used to describe a sequence of operations. The I2C transaction
 * is initiated when this register is written. The I2C module contains a status
 * register which in real-time tracks the progress of the I2C sequence that was
 * configured in the INST register. If enabled, an interrupt is generated when
 * the transaction is completed. If not using interrupts then bit 24 (INTB) of
 * the status register can be polled and for 0. INTB is the inverse of the i2cm
 * interrupt status.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "pmu.h"
#include "registers.h"
#include "system.h"
#include "timer.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/* Limits for polling I2C transaction */
#define I2CM_POLL_WAIT_US 25
#define I2CM_MAX_POLL_ITERATIONS (25000 / I2CM_POLL_WAIT_US)

/* Sizes for first write (FW) and read/write (RW) fifos */
#define I2CM_FW_BYTES_MAX 4
#define I2CM_RW_BYTES_MAX 64

/* Macros to set bits/fields of the INST word for sequences*/
#define I2CM_SET_START(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, START))
#define I2CM_SET_STOP(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, FINALSTOP))
#define I2CM_SET_RPT_START(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, \
							  REPEATEDSTART))
#define I2CM_SET_FWDEVADDR(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, \
							     FWDEVADDR))
#define I2CM_SET_DEVADDRVAL(inst, addr) (inst |= addr << GFIELD_LSB(I2C, INST, \
							     DEVADDRVAL))
#define I2CM_SET_RWDEVADDR(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, RWDEVADDR))
#define I2CM_SET_RWDEVADDR_RWB(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, \
							      RWDEVADDR_RWB))
#define I2CM_SET_NA(inst) (inst |= 1 << GFIELD_LSB(I2C, INST, FINALNA))
#define I2CM_SET_RWBYTES(inst, size) (inst |= size << GFIELD_LSB(I2C, INST, \
								 RWBYTESCOUNT))

/* Mask used to check status registers for errors */
#define I2CM_ERROR_MASK (~((1 << GFIELD_LSB(I2C, STATUS, INTB)) - 1))

enum i2cm_control_mode {
	disabled = 0,
	bit_bang = 1,
	instruction = 2,
	reserved = 3,
};

#define I2C_NUM_PHASESTEPS 4
struct i2c_xfer_mode {
	uint8_t clk_div;
	uint8_t phase_steps[I2C_NUM_PHASESTEPS];
};

const struct i2c_xfer_mode i2c_timing[I2C_FREQ_COUNT] = {
	/* 1000 kHz */
	{
		.clk_div = 1,
		.phase_steps = {5, 5, 5, 11},
	},
	/* 400 kHz */
	{
		.clk_div = 1,
		.phase_steps = {15, 12, 12, 21},
	},
	/* 100 kHz */
	{
		.clk_div = 10,
		.phase_steps = {6, 6, 8, 6},
	},
};

static void i2cm_config_xfer_mode(int port, enum i2c_freq freq)
{
	/* Set the phasesteps register for the requested bus frequency */
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P0,
		       i2c_timing[freq].phase_steps[0]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P1,
		       i2c_timing[freq].phase_steps[1]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P2,
		       i2c_timing[freq].phase_steps[2]);
	GWRITE_FIELD_I(I2C, port, CTRL_PHASESTEPS, P3,
		       i2c_timing[freq].phase_steps[3]);

	/* Set the clock divide control register */
	GWRITE_I(I2C, port, CTRL_CLKDIV, i2c_timing[freq].clk_div);

	/* Set the control mode register */
	GWRITE_I(I2C, port, CTRL_MODE, instruction);
}

static void i2cm_set_fwbytes(int port, uint32_t *inst, const uint8_t *data,
			     int size)
{
	int i;
	uint32_t fwbytes = 0;

	/* Indicate that first write bytes field will be used */
	*inst |= size << GFIELD_LSB(I2C, INST, FWBYTESCOUNT);

	/* Now write data to FWBYTES register */
	for (i = 0; i < size; i++)
		fwbytes |= data[i] << (i * 8);
	GWRITE_I(I2C, port, FW, fwbytes);
}

static void i2cm_write_rwbytes(int port, const uint8_t *out, int size)
{
	volatile uint32_t *rw_ptr;
	int rw_count;
	int byte_count;
	int i, j;
	uint32_t rw_data;

	/* Calculate number of RW register writes required */
	rw_count = (size + 3) >> 2;
	/* Get pointer to RW0 register (start of fifo) */
	rw_ptr = GREG32_ADDR_I(I2C, port, RW0);

	/*
	 * Get write data from source buffer one byte at a time and write up to
	 * 4 bytes at a time in to the RW fifo.
	 */
	for (i = 0; i < rw_count; i++) {
		rw_data = 0;
		byte_count = MIN(4, size);
		for (j = 0; j < byte_count; j++)
			rw_data |= *out++ << (j * 8);
		size -= byte_count;
		*rw_ptr++ = rw_data;
	}
}

static void i2cm_read_rwbytes(int port, uint8_t *in, int size)
{
	int rw_count;
	int byte_count;
	int i, j;
	uint32_t rw_data;
	volatile uint32_t *rw_ptr;

	/* Calculate number of RW register writes required */
	rw_count = (size + 3) >> 2;
	/* Get pointer to RW0 register (start of fifo) */
	rw_ptr = GREG32_ADDR_I(I2C, port, RW0);

	/*
	 * Read data from fifo up to 4 bytes at a time and copy into
	 * destination buffer 1 byte at a time.
	 */
	for (i = 0; i < rw_count; i++) {
		rw_data = *rw_ptr++;
		byte_count = MIN(4, size);
		for (j = 0; j < byte_count; j++) {
			*in++ = (rw_data & 0xff);
			rw_data >>= 8;
		}
		size -= byte_count;
	}
}

static int i2cm_poll_for_complete(int port)
{
	int poll_count = 0;

	while (poll_count < I2CM_MAX_POLL_ITERATIONS) {
		/* Check if the sequence is complete */
		if (!GREAD_FIELD_I(I2C, port, STATUS, INTB))
			return EC_SUCCESS;
		/* Not done yet, sleep */
		usleep(I2CM_POLL_WAIT_US);
		poll_count++;
	};

	return EC_ERROR_TIMEOUT;
}

static uint32_t i2cm_build_sequence(int port, int slave_addr,
			       const uint8_t *out,  int out_size,
			       uint8_t *in, int in_size, int flags)
{
	int bytes_consumed;
	uint32_t inst = 0;

	if (flags & I2C_XFER_START)
		I2CM_SET_START(inst);

	/* Setup slave device address */
	I2CM_SET_DEVADDRVAL(inst, slave_addr);

	if (out_size) {
		/* Send slave addr byte if this is start of I2C transaction */
		if (flags & I2C_XFER_START)
			I2CM_SET_FWDEVADDR(inst);
		bytes_consumed = MIN(I2CM_FW_BYTES_MAX, out_size);
		/* Setup first write bytes */
		i2cm_set_fwbytes(port, &inst, out, bytes_consumed);
		out_size -= bytes_consumed;
		/* If data remains, then put the rest in RW fifo */
		if (out_size) {
			out += bytes_consumed;
			I2CM_SET_RWBYTES(inst, out_size);
			i2cm_write_rwbytes(port, out, out_size);
		}
	}

	if (in_size) {
		/*
		 * If first read piece, then send slave address and indicate
		 * it's a read transaction.
		 */
		if (flags & I2C_XFER_START) {
			I2CM_SET_RWDEVADDR(inst);
			I2CM_SET_RWDEVADDR_RWB(inst);
			I2CM_SET_RPT_START(inst);
		}
		/* Setup number of bytes to read */
		I2CM_SET_RWBYTES(inst, in_size);

		/* NACK the last byte read */
		if (flags & I2C_XFER_STOP)
			I2CM_SET_NA(inst);
	}

	if (flags & I2C_XFER_STOP)
		I2CM_SET_STOP(inst);

	return inst;
}

static int i2cm_execute_sequence(int port, int slave_addr, const uint8_t *out,
				 int out_size, uint8_t *in, int in_size,
				 int flags)
{
	int rv;
	uint32_t inst;

	/* Build sequence instruciton */
	inst = i2cm_build_sequence(port, slave_addr, out, out_size, in,
				   in_size, flags);
	/* Start transaction */
	GWRITE_I(I2C, port, INST, inst);

	/* Wait for transaction to be complete */
	rv = i2cm_poll_for_complete(port);
	/* Handle timeout case */
	if (rv)
		return rv;

	/* Check status value for errors */
	if (GREAD_I(I2C, port, STATUS) & I2CM_ERROR_MASK) {
		/* If failed, then clear INST register */
		GWRITE_I(I2C, port, INST, 0);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

/*****************************************************************************
 * Exported functions declared in i2c.h
 */

/* Perform an i2c transaction. */
int chip_i2c_xfer(int port, int slave_addr, const uint8_t *out, int out_size,
		  uint8_t *in, int in_size, int flags)
{
	int rv;
	int sequence_flags;
	int num_out, num_in;

	if (!in_size && !out_size)
		/* Nothing to do */
		return EC_SUCCESS;

	/*
	 * Cr50 can do sequences of up to 64 write or read bytes. In addition it
	 * can accommodate up to 4 write bytes and up to 64 read bytes in a
	 * sequence set up. However, if the number of write bytes is > 4, then
	 * the write and read must be done in separate sequences.
	 */

	while (out_size > I2CM_FW_BYTES_MAX) {
		/* number of bytes that can handed in 1 sequence */
		num_out = MIN(I2CM_RW_BYTES_MAX + I2CM_FW_BYTES_MAX, out_size);
		/* If new out_size and in_size are 0, then copy flags */
		sequence_flags = flags;
		/* If more than 1 sequence remaining, mask stop bit flag */
		if ((out_size - num_out)  || in_size)
			sequence_flags &= ~I2C_XFER_STOP;
		/* Execute transaction */
		ccprintf("write only\n");
		rv = i2cm_execute_sequence(port, slave_addr, out, num_out, in,
					   0, sequence_flags);
		if (rv)
			return rv;
		/* Update counts and flags */
		out += num_out;
		out_size -= num_out;
		flags &= ~sequence_flags;
	}

	/* At this point out_size <= 4 */
	while (out_size || in_size) {
		num_in = MIN(I2CM_RW_BYTES_MAX, in_size);
		num_out = out_size;
		sequence_flags = flags;
		/* If more than 1 sequence remaining, mask stop bit flag */
		if (in_size - num_in)
			sequence_flags &= ~I2C_XFER_STOP;

		rv = i2cm_execute_sequence(port, slave_addr, out, num_out, in,
					   num_in, sequence_flags);
		if (rv)
			return rv;

		/* If bytes were read, copy to destination buffer */
		if (num_in) {
			i2cm_read_rwbytes(port, in, num_in);
			in += num_in;
			in_size -= num_in;
		}
		out_size = 0;
		flags &= ~sequence_flags;
	}

	return EC_SUCCESS;
}

int i2c_raw_get_scl(int port)
{
	return 0;
}

int i2c_raw_get_sda(int port)
{
	return 0;
}

int i2c_get_line_levels(int port)
{
	return 0;
}


static void i2cm_init_port(const struct i2c_port_t *p)
{
	enum i2c_freq freq;

	/* Enable clock for I2C Master */
	/* TODO (scollyer): Allow for both ports */
	pmu_clock_en(PERIPH_I2C);

	/* Set operation speed. */
	switch (p->kbps) {
	case 1000: /* Fast-mode Plus */
		freq = I2C_FREQ_1000KHZ;
		break;
	case 400: /* Fast-mode */
		freq = I2C_FREQ_400KHZ;
		break;
	case 100: /* Standard-mode */
		freq = I2C_FREQ_100KHZ;
		break;
	default: /* unknown speed, default to 100kBps */
		CPRINTS("I2C bad speed %d kBps.  Defaulting to 100kbps.",
			p->kbps);
		freq = I2C_FREQ_100KHZ;
	}

	/* Configure the transfer clocks and mode */
	i2cm_config_xfer_mode(p->port, freq);

	/* Clear any pending action */
	GWRITE_I(I2C, p->port, INST, 0);

	/* Enable I2C interrupt */
	GWRITE_I(I2C, p->port, CTRL_INT_EN, 1);

	CPRINTS("Initalized I2C port %d, freq = %d", p->port, p->kbps);
}

/**
 * Initialize the i2c module for all supported ports.
 */
static void i2cm_init(void)
{
	const struct i2c_port_t *p = i2c_ports;
	int i;

	for (i = 0; i < i2c_ports_used; i++, p++)
		i2cm_init_port(p);

}
DECLARE_HOOK(HOOK_INIT, i2cm_init, HOOK_PRIO_INIT_I2C);


static void display_status(uint32_t status)
{
	int start, stop, fwdevaddr;
	int fwbytes, scl0, rep_start;
	int rwdevaddr, rwbytes, final_na;
	int rwbit, hold, finalstop, devaddr;

	start = (status & GFIELD_MASK(I2C, INST, START)) >>
		GFIELD_LSB(I2C, INST, START);
	stop = (status & GFIELD_MASK(I2C, INST, FIRSTSTOP)) >>
		GFIELD_LSB(I2C, INST, FIRSTSTOP);
	fwdevaddr = (status & GFIELD_MASK(I2C, INST, FWDEVADDR)) >>
		GFIELD_LSB(I2C, INST, FWDEVADDR);
	fwbytes = (status & GFIELD_MASK(I2C, INST, FWBYTESCOUNT)) >>
		GFIELD_LSB(I2C, INST, FWBYTESCOUNT);
	scl0 = (status & GFIELD_MASK(I2C, INST, SCL0)) >>
		GFIELD_LSB(I2C, INST, SCL0);
	rep_start = (status & GFIELD_MASK(I2C, INST, REPEATEDSTART)) >>
		GFIELD_LSB(I2C, INST, REPEATEDSTART);
	rwdevaddr = (status & GFIELD_MASK(I2C, INST, RWDEVADDR)) >>
		GFIELD_LSB(I2C, INST, RWDEVADDR);
	rwbytes = (status & GFIELD_MASK(I2C, INST, RWBYTESCOUNT)) >>
		GFIELD_LSB(I2C, INST, RWBYTESCOUNT);
	final_na = (status & GFIELD_MASK(I2C, INST, FINALNA)) >>
		GFIELD_LSB(I2C, INST, FINALNA);
	rwbit = (status & GFIELD_MASK(I2C, INST, RWBIT)) >>
		GFIELD_LSB(I2C, INST, RWBIT);
	hold = (status & GFIELD_MASK(I2C, INST, HOLD0)) >>
		GFIELD_LSB(I2C, INST, HOLD0);
	finalstop = (status & GFIELD_MASK(I2C, INST, FINALSTOP)) >>
		GFIELD_LSB(I2C, INST, FINALSTOP);
	devaddr = (status & GFIELD_MASK(I2C, INST, DEVADDRVAL)) >>
		GFIELD_LSB(I2C, INST, DEVADDRVAL);

	ccprintf("inst/status = 0x%x\n", status);
	ccprintf("start = %d, rep_start = %d\n", start, rep_start);
	ccprintf("fwdevaddr = %d, fwbytes = %d\n", fwdevaddr, fwbytes);
	ccprintf("rwdevaddr = %d, rwbytes = %d\n", rwdevaddr, rwbytes);
	ccprintf("final_na = %d, rwbit = %d, hold = %d\n", final_na,
		rwbit, hold);
	ccprintf("first stop = %d, final_stop = %d\n", stop, finalstop);
	ccprintf("devaddr = 0x%x, sclo = %d\n", devaddr, scl0);

}

static void command_dumpregs(void)
{
	ccprintf("PMU clock = 0x%x\n", GREAD(PMU, PERICLKSET0));
	ccprintf("CTRL_MODE = 0x%x\n", GREAD(I2C, CTRL_MODE));
	ccprintf("CTRL_CLKDIV = 0x%x\n", GREAD(I2C, CTRL_CLKDIV));
	ccprintf("CTRL_PHASE 0 = %d. 1 = %d, 2 = %d, 3= %d\n",
		 GREAD_FIELD(I2C, CTRL_PHASESTEPS, P0),
		 GREAD_FIELD(I2C, CTRL_PHASESTEPS, P1),
		 GREAD_FIELD(I2C, CTRL_PHASESTEPS, P2),
		 GREAD_FIELD(I2C, CTRL_PHASESTEPS, P3));
	ccprintf("CTRL_INT_EN = 0x%x\n", GREAD(I2C, CTRL_INT_EN));
	ccprintf("INST = 0x%x\n", GREAD(I2C, INST));
	display_status(GREAD(I2C, INST));
	ccprintf("STATUS = 0x%x\n", GREAD(I2C, STATUS));
	ccprintf("SCL_SDA = 0x%x\n", GREAD(I2C, READVAL));
	ccprintf("CTRL_CS = 0x%x\n", GREAD(I2C, CTRL_CS));
	ccprintf("CTRL_MSR = 0x%x\n", GREAD(I2C, CTRL_MSR));
}

static void cmd_wiggle_i2c(void)
{
	int sda, scl;
	int addr = 0xa5;
	int i;
	/* init condition */
	sda = 1;
	scl = 1;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(1);
	/* clk low */
	scl = 0;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(1);
	/* sda low (start bit) */
	sda = 0;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(1);
	/* clock high */
	scl = 1;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(1);

	/* send device address */
	for (i = 0; i < 8; i++) {
		/* clock low */
		scl = 0;
		GWRITE(I2C, INST, (scl << 1) | sda);
		usleep(1);
		/* set sda */
		sda = addr & 0x80 ? 1 : 0;
		addr <<= 1;
		GWRITE(I2C, INST, (scl << 1) | sda);
		/*clock high */
		scl = 1;
		GWRITE(I2C, INST, (scl << 1) | sda);
		usleep(1);
	}

	scl = 0;
	sda = 1;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(1);
	scl = 1;
	sda = 1;
	GWRITE(I2C, INST, (scl << 1) | sda);
	usleep(5);

}

static void test_i2cm(void)
{
	int reg_address;
	int reg_value;
	uint8_t out[3];
	uint8_t in[2];
	int i;
	int flags;
	int slave_addr;
	int rv;

	/* Chan 1 crit alert */
	reg_address = 14;
	reg_value = 0xa5fe;
	out[0] = reg_address;
	out[1] = reg_value >> 8;
	out[2] = reg_value & 0xff;

	/* Write to chan 1 crit alert */
	chip_i2c_xfer(i2c_ports[0].port, 0x41, out,
			      3, in, 0, I2C_XFER_SINGLE);
	/* Read chan crit alert */
	chip_i2c_xfer(i2c_ports[0].port, 0x41, out,
			      1, in, 2, I2C_XFER_SINGLE);
	ccprintf("i2cm: ina reg %d, wr = 0x%x, rd = 0x%x%x\n",
		 reg_address, reg_value, in[0], in[1]);


	reg_address = 14;
	reg_value = 0x5aff;
	out[0] = reg_address;
	out[1] = reg_value >> 8;
	out[2] = reg_value & 0xff;

	flags = I2C_XFER_START;
	slave_addr = 0x42;
	for (i = 0; i < 3; i++) {
		if (i == 2)
			flags |= I2C_XFER_STOP;
		/* Write to chan 2 warning alert */
		rv = chip_i2c_xfer(i2c_ports[0].port, slave_addr, &out[i],
			      1, in, 0, flags);
		if (rv)
			ccprintf("i2cm: failed %d byte write\n", i);
		flags = 0;
	}

	rv = chip_i2c_xfer(i2c_ports[0].port, slave_addr, &out[0],
			      0, in, 2, I2C_XFER_START | I2C_XFER_STOP);

	ccprintf("i2cm: ina reg %d, wr = 0x%x, rd = 0x%x%x\n",
		 reg_address, reg_value, in[0], in[1]);
}

static int command_i2cdbg(int argc, char **argv)
{
	int reg_addr;
	int mode;
	int scl, sda;
	char *e;
	uint8_t reg;
	uint8_t buf[2];

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "reg")) {
		command_dumpregs();
	} else if (!strcasecmp(argv[1], "rd")) {
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		reg_addr = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		if ((reg_addr < 0) || (reg_addr >= 256))
			return EC_ERROR_PARAM1;

		ccprintf("i2cdbg: reg address = 0x%x\n", reg_addr);
		GWRITE(I2C, INST, 0);
		GWRITE(I2C, CTRL_MODE, instruction);
		/* Set register pointer */
		reg = reg_addr;
		chip_i2c_xfer(i2c_ports[0].port, 0x40, (const uint8_t *)&reg,
			      1, buf, 0, I2C_XFER_SINGLE);
		ccprintf("i2cbdg: Finished write, status = 0x%x\n",
		GREAD(I2C, STATUS));
		GWRITE(I2C, INST, 0);
		/* Read register */
		chip_i2c_xfer(i2c_ports[0].port, 0x40, (const uint8_t *)&reg,
			      0, buf, 2, I2C_XFER_SINGLE);
		ccprintf("i2cdbg: reg %d read, val = 0x%x, status = 0x%x\n",
		reg, GREAD(I2C, RW0), GREAD(I2C, STATUS));

		/* Try in the same transaction */
		chip_i2c_xfer(i2c_ports[0].port, 0x41, (const uint8_t *)&reg,
			      1, buf, 2, I2C_XFER_SINGLE);
		ccprintf("i2cdbg: reg %d read, val = 0x%x%x, status = 0x%x\n",
			 reg, buf[0], buf[1], GREAD(I2C, STATUS));

	}  else if (!strcasecmp(argv[1], "md")) {
		if (argc < 3)
			return EC_ERROR_PARAM_COUNT;
		mode = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		if ((reg_addr < 0) || (reg_addr >= 4))
			return EC_ERROR_PARAM1;

		/* Set the control mode register */
		ccprintf("Setting mode to %d\n", mode);
		GWRITE(I2C, CTRL_MODE, mode);
	} else if (!strcasecmp(argv[1], "bang")) {
		if (argc < 4)
			return EC_ERROR_PARAM_COUNT;
		scl = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		if ((scl < 0) || (scl >= 2))
			return EC_ERROR_PARAM1;
		sda = strtoi(argv[3], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		if ((sda < 0) || (sda >= 2))
			return EC_ERROR_PARAM1;

		ccprintf("Bang: scl = %d, sda = %d\n", scl, sda);
		GWRITE(I2C, CTRL_MODE, 1);
		GWRITE(I2C, INST, (scl << 1) | sda);
	} else if (!strcasecmp(argv[1], "wg")) {
		GWRITE(I2C, CTRL_MODE, 1);
		cmd_wiggle_i2c();
	} else if (!strcasecmp(argv[1], "test")) {
		test_i2cm();
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2cm, command_i2cdbg, NULL, NULL);
