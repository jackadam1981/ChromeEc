/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C module for Chrome EC */

#include "gpio.h"
#include "hooks.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "compile_time_macros.h"

#define I2C_READ_MAXFIFO_DATA 16

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
struct i2c_slv_info {
	const char *name;     /* Port name */
	uint8_t slave_adr;    /* slave address */
	uint8_t slave_adr2;   /* slave address2 */
	int irq;              /* slave irq */
};

/* I2C slave ports */
const struct i2c_slv_info i2c_slv[] = {
	[IT83XX_I2C_CH_A] = {"evb-a", 0x52, 0x00, IT83XX_IRQ_SMB_A},
#if 0
	[IT83XX_I2C_CH_D] = {"evb-d", 0x60, 0x00, IT83XX_IRQ_SMB_D},
	[IT83XX_I2C_CH_E] = {"evb-e", 0x62, 0x00, IT83XX_IRQ_SMB_E},
	[IT83XX_I2C_CH_F] = {"evb-f", 0x64, 0x00, IT83XX_IRQ_SMB_F},
#endif
};

const unsigned int i2c_slvs = ARRAY_SIZE(i2c_slv);

void i2c_slave_isr(int port)
{
	uint8_t l_buffer __attribute__((unused)) = 0;
	int i, count;

	if (port < I2C_STANDARD_PORT_COUNT) {
		/* FIFO byte count */
		count = IT83XX_SMB_SFFSTA & 0x1f;

		/* Slave data register is waiting for read or write. */
		if (IT83XX_SMB_SLSTA(port) & 0x02) {
			/* Master read data */
			if (IT83XX_SMB_SLSTA(port) & 0x08) {

				for (i = 0; i < I2C_READ_MAXFIFO_DATA; i++)
					/* Return buffer data to master */
					IT83XX_SMB_SLDA(port) =
						pbuffer[i + read_index];

				/* Write clear the slave data status */
				IT83XX_SMB_SLSTA(port) = 0x02;

				/* Index to next 16 bytes of read buffer */
				read_index += I2C_READ_MAXFIFO_DATA;
			}
			/* Master write data */
			else {
				/* FIFO Full */
				if (IT83XX_SMB_SFFSTA & 0x40) {
					for (i = 0; i < count; i++)
				/* Get data from master to buffer */
						pbuffer[write_index + i] =
							IT83XX_SMB_SLDA(port);
				}
				/* Write clear the slave data status */
				IT83XX_SMB_SLSTA(port) = 0x02;

				/* To release clock pin */
				l_buffer = IT83XX_SMB_SLDA(port);

				/* Index to next byte of write buffer */
				write_index += count;
			}
		}
		/* Stop condition, indicate stop condition detected. */
		else if (IT83XX_SMB_SLSTA(port) & 0x20) {
			/* Write clear the stop condition */
			IT83XX_SMB_SLSTA(port) = 0x20;

			for (i = 0; i < count; i++) {
				/* Master read data */
				if (IT83XX_SMB_SLSTA(port) & 0x08) {
					/* Write data from buffer to master */
					IT83XX_SMB_SLDA(port) =
						pbuffer[i + read_index];
				}
				/* Master write data */
				else {
					/* Get data from master to buffer */
					pbuffer[i + write_index] =
							IT83XX_SMB_SLDA(port);
				}
			}
			/* Reset w/r buffer index */
			buffer_index_reset();
		}
		/* Slave time status, timeout status occurs. */
		else if (IT83XX_SMB_SLSTA(port) & 0x04) {
			/* Write clear the timeout status */
			IT83XX_SMB_SLSTA(port) = 0x04;

			/* Reset w/r buffer index */
			buffer_index_reset();
		}
	}
}

void i2c_slv_interrupt(int port)
{

	/* Write clear the isr[x] */
	task_clear_pending_irq(i2c_slv[port].irq);

	/* Slave ISR */
	i2c_slave_isr(port);

}

void i2c_slave_enable(int port, uint8_t slv_addr1, uint8_t slv_addr2)
{
	if (port < I2C_STANDARD_PORT_COUNT) {

		/* bit5 : SMBus slave A enable */
		IT83XX_SMB_HOCTL2(port) = 0x20;

		/* bit0 : Slave A FIFO Enable */
		IT83XX_SMB_SFFCTL = 0x01;

		/*
		 * bit0 : host notify interrupt enable.
		 * bit1 : slave interrupt enable.
		 * bit2 : SMCLK/SMDAT will be released if timeout.
		 * bit3 : slave detect STOP condition interrupt enable.
		 */
		IT83XX_SMB_SICR(port) = 0x0f;

		/* Slave address 1 */
		IT83XX_SMB_RESLADR(port) = slv_addr1;

		/*
		 * Slave address 2
		 * bit7 : SADR2 field is valided.
		 */
		if (slv_addr2 != 0)
			IT83XX_SMB_RESLADR2(port) = (slv_addr2 + 0x80);

		/*
		 * Kill the current host transaction.
		 * This bit once set, has to be cleared
		 * by software to allow the SMBus Host
		 * controller to function normally.
		 */
		IT83XX_SMB_HOCTL(port) = 0x02;
		IT83XX_SMB_HOCTL(port) = 0x00;

		/* Write clear all master status */
		IT83XX_SMB_HOSTA(port) = 0xFF;

		/* Write clear all slave status */
		IT83XX_SMB_SLSTA(port) = 0xFF;
	}
}

static void i2c_slave_init(void)
{
	int  i;

	/* Configure GPIOs */
	gpio_config_module(MODULE_I2C, 1);

	/* Enable I2C Slave function */
	for (i = 0; i < i2c_slvs; i++) {

		/* Slave interface select register */
		IT83XX_SMB_SLVISELR &= ~0x10;

		if (i2c_slv[i].name) {
			/* Write clear the isr[x] and enable ier[x] */
			task_clear_pending_irq(i2c_slv[i].irq);
			task_enable_irq(i2c_slv[i].irq);

			/* To enable slave ch[x] */
			i2c_slave_enable(i,	i2c_slv[i].slave_adr,
				i2c_slv[i].slave_adr2);
		}
	}

	/* Slave global setting for 100K */
	IT83XX_SMB_25MS = 0x19;
}
DECLARE_HOOK(HOOK_INIT, i2c_slave_init, HOOK_PRIO_INIT_I2C);
