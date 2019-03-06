/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C module for Chrome EC */

#include <stddef.h>
#include "gpio.h"
#include "hooks.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "compile_time_macros.h"
#include "i2cs.h"

#define I2C_READ_MAXFIFO_DATA 16
/* The size must be a power of 2 */
#define I2C_MAX_BUFFER_SIZE 0x100
#define I2C_SIZE_MASK (I2C_MAX_BUFFER_SIZE - 1)

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

uint32_t w_index;
uint32_t r_index;
uint8_t pbuffer[I2C_MAX_BUFFER_SIZE];

void buffer_index_reset(void)
{
	/* Reset write buffer index */
	w_index = 0;
	/* Reset read buffer index */
	r_index = 0;
}

/* Data structure to define I2C slave control configuration. */
struct i2c_slv_ctrl_t {
	int irq;              /* slave irq */

};

/* I2C slave control */
const struct i2c_slv_ctrl_t i2c_slv_ctrl[] = {
	[IT83XX_I2C_CH_A] = {IT83XX_IRQ_SMB_A},
#if 0
	[IT83XX_I2C_CH_D] = {IT83XX_IRQ_SMB_D},
	[IT83XX_I2C_CH_E] = {IT83XX_IRQ_SMB_E},
	[IT83XX_I2C_CH_F] = {IT83XX_IRQ_SMB_F},
#endif
};

const unsigned int i2c_slvs_used = ARRAY_SIZE(i2c_slv_ctrl);

void i2c_slave_read_write_data(int port)
{
	int i, count, slv_status;

	slv_status = IT83XX_SMB_SLSTA;

	if (port < I2C_STANDARD_PORT_COUNT) {
		/* bit0-4 : FIFO byte count */
		count = IT83XX_SMB_SFFSTA & 0x1F;

		/* Slave data register is waiting for read or write. */
		if (slv_status & IT83XX_SMB_SDS) {
			/* Master to read data */
			if (slv_status & IT83XX_SMB_RCS) {
				for (i = 0; i < I2C_READ_MAXFIFO_DATA; i++)
					/* Return buffer data to master */
					IT83XX_SMB_SLDA =
					pbuffer[(i + r_index) & I2C_SIZE_MASK];

				/* Index to next 16 bytes of read buffer */
				r_index += I2C_READ_MAXFIFO_DATA;
			}
			/* Master to write data */
			else {
				/* FIFO Full */
				if (IT83XX_SMB_SFFSTA & IT83XX_SMB_SFFFULL) {
					for (i = 0; i < count; i++)
				/* Get data from master to buffer */
						pbuffer[(w_index + i) &
					I2C_SIZE_MASK] = IT83XX_SMB_SLDA;
				}

				/* Index to next byte of write buffer */
				w_index += count;
			}
		}
		/* Stop condition, indicate stop condition detected. */
		if (slv_status & IT83XX_SMB_SPDS) {
			/* Read data less 16 bytes status */
			if (slv_status & IT83XX_SMB_RCS) {
				/* Disable FIFO mode to clear left count */
				IT83XX_SMB_SFFCTL &= ~IT83XX_SMB_SAFE;

				/* Slave A FIFO Enable */
				IT83XX_SMB_SFFCTL |= IT83XX_SMB_SAFE;
			}
			/* Master to write data */
			else {
				for (i = 0; i < count; i++)
					/* Get data from master to buffer */
					pbuffer[(i + w_index) &
					I2C_SIZE_MASK] = IT83XX_SMB_SLDA;
			}

			/* Reset read and write buffer index */
			buffer_index_reset();
		}
		/* Slave time status, timeout status occurs. */
		if (slv_status & IT83XX_SMB_STS) {
			/* Reset read and write buffer index */
			buffer_index_reset();
		}

		/* Write clear the slave status */
		IT83XX_SMB_SLSTA = slv_status;
	}
}

void i2c_slv_interrupt(int port)
{
	/* Slave to read and write fifo data */
	i2c_slave_read_write_data(port);

	/* Clear the interrupt status */
	task_clear_pending_irq(i2c_slv_ctrl[port].irq);
}

void i2c_slave_enable(int port, uint8_t slv_addr1, uint8_t slv_addr2)
{
	if (port < I2C_STANDARD_PORT_COUNT) {

		/* This field defines the SMCLK0/1/2 clock/data low timeout. */
		IT83XX_SMB_25MS = I2C_CLK_LOW_TIMEOUT;

		/* bit0 : Slave A FIFO Enable */
		IT83XX_SMB_SFFCTL |= IT83XX_SMB_SAFE;

		/*
		 * bit1 : Slave interrupt enable.
		 * bit2 : SMCLK/SMDAT will be released if timeout.
		 * bit3 : Slave detect STOP condition interrupt enable.
		 */
		IT83XX_SMB_SICR = 0x0E;

		/* Slave address 1 */
		IT83XX_SMB_RESLADR = slv_addr1;

		/*
		 * Slave address 2
		 * bit7 : SADR2 field is valided.
		 */
		if (slv_addr2)
			IT83XX_SMB_RESLADR2 =
				IT83XX_SMB_ENADDR2 | slv_addr2;

		/* Write clear all slave status */
		IT83XX_SMB_SLSTA = 0xE7;

		/* bit5 : Enable the SMBus slave device */
		IT83XX_SMB_HOCTL2(port) |= IT83XX_SMB_SLVEN;
	}
}

static void i2c_slave_init(void)
{
	int  i;

	/* Enable I2C Slave function */
	for (i = 0; i < i2c_slvs_used; i++) {

		if (i2c_slv_ports[i].name != NULL) {
			/* To enable slave ch[x] */
			i2c_slave_enable(i, i2c_slv_ports[i].slave_adr,
				i2c_slv_ports[i].slave_adr2);

			/* Clear the interrupt status */
			task_clear_pending_irq(i2c_slv_ctrl[i].irq);

			/* enable i2c interrupt */
			task_enable_irq(i2c_slv_ctrl[i].irq);

		}
	}
}
DECLARE_HOOK(HOOK_INIT, i2c_slave_init, HOOK_PRIO_INIT_I2C);
