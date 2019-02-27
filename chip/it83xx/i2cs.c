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
#define I2C_CLK_LOW_TIMEOUT  255 /* ~=249 ms */

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

uint32_t write_index;
uint32_t read_index;
uint8_t pbuffer[256];

void buffer_index_reset(void)
{
	/* Reset write buffer index */
	write_index = 0x00;
	/* Reset read buffer index */
	read_index = 0x00;
}

/* Data structure to define I2C slave port configuration. */
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
	uint8_t l_buffer __attribute__((unused)) = 0;
	int i, count;

	if (port < I2C_STANDARD_PORT_COUNT) {
		/* FIFO byte count */
		count = IT83XX_SMB_SFFSTA & 0x1F;

		/* Slave data register is waiting for read or write. */
		if (IT83XX_SMB_SLSTA & IT83XX_SMB_SDS) {
			/* Master read data */
			if (IT83XX_SMB_SLSTA & IT83XX_SMB_RCS) {

				for (i = 0; i < I2C_READ_MAXFIFO_DATA; i++)
					/* Return buffer data to master */
					IT83XX_SMB_SLDA =
						pbuffer[i + read_index];

				/* Write clear the slave data status */
				IT83XX_SMB_SLSTA |= IT83XX_SMB_SDS;

				/* Index to next 16 bytes of read buffer */
				read_index += I2C_READ_MAXFIFO_DATA;
			}
			/* Master write data */
			else {
				/* FIFO Full */
				if (IT83XX_SMB_SFFSTA & IT83XX_SMB_SFFFULL) {
					for (i = 0; i < count; i++)
				/* Get data from master to buffer */
						pbuffer[write_index + i] =
							IT83XX_SMB_SLDA;
				}
				/* Write clear the slave data status */
				IT83XX_SMB_SLSTA |= IT83XX_SMB_SDS;

				/* To release clock pin */
				l_buffer = IT83XX_SMB_SLDA;

				/* Index to next byte of write buffer */
				write_index += count;
			}
		}
		/* Stop condition, indicate stop condition detected. */
		else if (IT83XX_SMB_SLSTA & IT83XX_SMB_SPDS) {
			/* Write clear the stop condition */
			IT83XX_SMB_SLSTA |= IT83XX_SMB_SPDS;

			for (i = 0; i < count; i++) {
				/* Master read data */
				if (IT83XX_SMB_SLSTA & IT83XX_SMB_RCS) {
					/* Write data from buffer to master */
					IT83XX_SMB_SLDA =
						pbuffer[i + read_index];
				}
				/* Master write data */
				else {
					/* Get data from master to buffer */
					pbuffer[i + write_index] =
							IT83XX_SMB_SLDA;
				}
			}
			/* Reset w/r buffer index */
			buffer_index_reset();
		}
		/* Slave time status, timeout status occurs. */
		else if (IT83XX_SMB_SLSTA & IT83XX_SMB_STS) {
			/* Write clear the timeout status */
			IT83XX_SMB_SLSTA |= IT83XX_SMB_STS;

			/* Reset w/r buffer index */
			buffer_index_reset();
		}
	}
}

void i2c_slv_interrupt(int port)
{
	/* Clear the interrupt status */
	task_clear_pending_irq(i2c_slv_ctrl[port].irq);

	/* Slave to read and write fifo data */
	i2c_slave_read_write_data(port);
}

void i2c_slave_enable(int port, uint8_t slv_addr1, uint8_t slv_addr2)
{
	if (port < I2C_STANDARD_PORT_COUNT) {

		/* This field defines the SMCLK0/1/2 clock/data low timeout. */
		IT83XX_SMB_25MS = I2C_CLK_LOW_TIMEOUT;

		/* bit5 : SMBus slave A enable */
		IT83XX_SMB_HOCTL2(port) |= IT83XX_SMB_SLVEN;

		/* bit0 : Slave A FIFO Enable */
		IT83XX_SMB_SFFCTL |= IT83XX_SMB_SAFE;

		/*
		 * bit0 : host notify interrupt enable.
		 * bit1 : slave interrupt enable.
		 * bit2 : SMCLK/SMDAT will be released if timeout.
		 * bit3 : slave detect STOP condition interrupt enable.
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
		IT83XX_SMB_SLSTA = 0xFF;
	}
}

static void i2c_slave_init(void)
{
	int  i;

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);

	/* Enable I2C Slave function */
	for (i = 0; i < i2c_slvs_used; i++) {

		if (i2c_slv_ports[i].name != NULL) {
			/* Clear the interrupt status */
			task_clear_pending_irq(i2c_slv_ctrl[i].irq);

			/* enable i2c interrupt */
			task_enable_irq(i2c_slv_ctrl[i].irq);

			/* To enable slave ch[x] */
			i2c_slave_enable(i, i2c_slv_ports[i].slave_adr,
				i2c_slv_ports[i].slave_adr2);
		}
	}
}
DECLARE_HOOK(HOOK_INIT, i2c_slave_init, HOOK_PRIO_INIT_I2C);
