/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C module for Chrome EC */

#include "clock.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2cs.h"
#include "registers.h"
#include <stddef.h>
#include <string.h>
#include "task.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

#define I2C_READ_MAXFIFO_DATA 16
/* The size must be a power of 2 */
#define I2C_MAX_BUFFER_SIZE 0x100
#define I2C_SIZE_MASK (I2C_MAX_BUFFER_SIZE - 1)

/* Store master to slave data of channel D, E, F by DMA */
static uint8_t in_data[3][I2C_MAX_BUFFER_SIZE]
			__attribute__((section(".h2ram.pool.i2cslv")));
/* Store slave to master data of channel D, E, F by DMA */
static uint8_t out_data[3][I2C_MAX_BUFFER_SIZE]
			__attribute__((section(".h2ram.pool.i2cslv")));
/* Store read and write data of channel A by FIFO mode */
static uint8_t pbuffer[I2C_MAX_BUFFER_SIZE];

uint32_t w_index;
uint32_t r_index;

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
	int reg_shift;
	enum clock_gate_offsets clock_gate;
	int dma_index;
};

/* I2C slave control */
const struct i2c_slv_ctrl_t i2c_slv_ctrl[] = {
	[IT83XX_I2C_CH_A] = {IT83XX_IRQ_SMB_A, -1, CGC_OFFSET_SMBA, -1},
	[IT83XX_I2C_CH_D] = {IT83XX_IRQ_SMB_D, 3, CGC_OFFSET_SMBD, 0},
	[IT83XX_I2C_CH_E] = {IT83XX_IRQ_SMB_E, 0, CGC_OFFSET_SMBE, 1},
	[IT83XX_I2C_CH_F] = {IT83XX_IRQ_SMB_F, 1, CGC_OFFSET_SMBF, 2},
};

void i2c_slave_read_write_data(int port)
{
	/* I2C slave channel A FIFO mode */
	if (port < I2C_STANDARD_PORT_COUNT) {
		int i, count, slv_status;

		slv_status = IT83XX_SMB_SLSTA;

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
	/* I2C slave channel D, E, F DMA mode */
	else {
		int r_sh;

		/* Shift register */
		r_sh = i2c_slv_ctrl[port].reg_shift;

		/* Interrupt pending */
		if (IT83XX_I2C_STR(r_sh) & IT83XX_I2C_INTPEND) {

			if (!(IT83XX_I2C_IRQ_ST(r_sh) & IT83XX_I2C_P_CLR)) {
				/* Master to read data */
				if (IT83XX_I2C_IRQ_ST(r_sh)
						& IT83XX_I2C_IDR_CLR) {
					IT83XX_I2C_IRQ_ST(r_sh)
						= IT83XX_I2C_IDR_CLR;
					CPRINTS("TODO:data in out_data buffer");
				}
				/* Master to write data */
				else {
					IT83XX_I2C_IRQ_ST(r_sh)
						= IT83XX_I2C_IDW_CLR;
					CPRINTS("TODO:data in in_data buffer");
				}
			}
			/* Slave finish */
			else
				IT83XX_I2C_IRQ_ST(r_sh) = IT83XX_I2C_P_CLR
				| IT83XX_I2C_SLVDATAFLG;
		}

		/* Hardware reset */
		IT83XX_I2C_CTR(r_sh) |= IT83XX_I2C_HALT;
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

	clock_enable_peripheral(i2c_slv_ctrl[port].clock_gate, 0, 0);

	/* I2C slave channel A FIFO mode */
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
	/* I2C slave channel D, E, F DMA mode */
	else {
		int r_sh, idx;

		/* Shift register */
		r_sh = i2c_slv_ctrl[port].reg_shift;

		idx = i2c_slv_ctrl[port].dma_index;

		/* Bit stretching */
		IT83XX_I2C_TOS(r_sh) |= IT83XX_I2C_CLK_STR;

		/* Slave address(8-bit)*/
		IT83XX_I2C_IDR(r_sh) = slv_addr1 << 1;

		/* Slave address2(8-bit) */
		if (slv_addr2)
			IT83XX_I2C_IDR2(r_sh) = slv_addr2 << 1;

		/* I2C module enable and command queue mode */
		IT83XX_I2C_CTR1(r_sh) = IT83XX_I2C_COMQ_EN
		| IT83XX_I2C_MDL_EN;

		/* I2C interrupt enable and set acknowledge */
		IT83XX_I2C_CTR(r_sh) = IT83XX_I2C_HALT
		| IT83XX_I2C_INTEN | IT83XX_I2C_ACK;

		/*
		 * bit3 : Slave ID write flag
		 * bit2 : Slave ID read flag
		 * bit1 : Slave received data flag
		 * bit0 : Slave finish
		 */
		IT83XX_I2C_IRQ_ST(r_sh) = 0xFF;

		/* Clear read and write data buffer of DMA */
		memset(in_data[idx], 0, I2C_MAX_BUFFER_SIZE);
		memset(out_data[idx], 0, I2C_MAX_BUFFER_SIZE);

		/* DMA write target address register */
		IT83XX_I2C_RAMHA(r_sh) = ((uint32_t)in_data[idx]>>8) & 0xFF;
		IT83XX_I2C_RAMLA(r_sh) = (uint32_t)in_data[idx] & 0xFF;

		/* DMA read target address register */
		IT83XX_I2C_RAMHA2(r_sh) = ((uint32_t)out_data[idx]>>8) & 0xFF;
		IT83XX_I2C_RAMLA2(r_sh) = (uint32_t)out_data[idx] & 0xFF;

		switch (port) {
		case IT83XX_I2C_CH_D:
			/* Enable I2C D channel */
			IT83XX_GPIO_GRC2 |= 0x20;
			break;
		case IT83XX_I2C_CH_E:
			/* Enable I2C E channel */
			IT83XX_GCTRL_PMER1 |= 0x01;
			break;
		case IT83XX_I2C_CH_F:
			/* Enable I2C F channel */
			IT83XX_GCTRL_PMER1 |= 0x02;
			break;
		}
	}
}

static void i2c_slave_init(void)
{
	int  i, p;

	/* Enable I2C Slave function */
	for (i = 0; i < i2c_slvs_used; i++) {

		/* I2c slave port mapping. */
		p = i2c_slv_ports[i].port;

		/* To enable slave ch[x] */
		i2c_slave_enable(p, i2c_slv_ports[i].slave_adr,
			i2c_slv_ports[i].slave_adr2);

		/* Clear the interrupt status */
		task_clear_pending_irq(i2c_slv_ctrl[p].irq);

		/* enable i2c interrupt */
		task_enable_irq(i2c_slv_ctrl[p].irq);
	}
}
DECLARE_HOOK(HOOK_INIT, i2c_slave_init, HOOK_PRIO_INIT_I2C + 1);
