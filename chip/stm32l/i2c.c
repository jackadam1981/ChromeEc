/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C slave driver */

#include <stdint.h>

#include "board.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "uart.h"

/* 8-bit I2C slave address */
#define I2C_ADDRESS 0xEC

/* I2C bus frequency */
#define I2C_FREQ 100000 /* Hz */

/* Clock divider for I2C controller */
#define I2C_CCR (CPU_CLOCK/(2 * I2C_FREQ))

static uint8_t i2c_read_reg(uint8_t reg)
{
	/* Dummy implementation : return inverted value  */
	return reg ^ 0xff;
}

static void i2c_write_reg(uint8_t reg, uint8_t value)
{
	/* Dummy implementation : just display it */
	uart_printf("reg %02x = %02x\n", reg, value);
}

static void i2c2_interrupt(void)
{
	uint32_t stat1 = STM32L_I2C_SR1(2);
	static int reg;
	static int first_write;

	/* clear status */
	STM32L_I2C_SR1(2) = 0;

	/* transfer matched our slave address */
	if (stat1 & (1 << 1)) {
		STM32L_I2C_SR2(2);
		first_write = 1;
	}
	/* write from the AP */
	if (stat1 & (1 << 6)) {
		/* the AP writes the register index */
		if (first_write) {
			reg = STM32L_I2C_DR(2);
			first_write = 0;
		} else {
			uint8_t val = STM32L_I2C_DR(2);
			i2c_write_reg(reg, val);
		}
	}
	/* read from the AP */
	if (stat1 & (1 << 7)) {
		STM32L_I2C_DR(2) = i2c_read_reg(reg);
	}
	/* receive stop bit */
	if (stat1 & (1 << 4)) {
		STM32L_I2C_SR2(2);
		STM32L_I2C_CR1(2) |= 1;
	}
}
DECLARE_IRQ(STM32L_IRQ_I2C2_EV, i2c2_interrupt, 1);
DECLARE_IRQ(STM32L_IRQ_I2C2_ER, i2c2_interrupt, 1);

int i2c_init2(int argc, char **argv)
{
	/* Enable I2C2 clock */
	STM32L_RCC_APB1ENR |= 1 << 22;

	/* Set clock configuration : standard mode (100kHz) */
	STM32L_I2C_CCR(2) = I2C_CCR;

	/* set slave address */
	STM32L_I2C_OAR1(2) = I2C_ADDRESS;

	/* Configuration : I2C mode / Periphal enabled / automatic ACK */
	STM32L_I2C_CR1(2) = (1 << 10) | (1 << 0);
	/* Error and event interrupts enabled / input clock is 16Mhz */
	STM32L_I2C_CR2(2) = (1 << 9) | (1 << 8) | (CPU_CLOCK / 1000000);

	/* clear status */
	STM32L_I2C_SR1(2) = 0;

	/* Enable timer interrupts */
	task_enable_irq(STM32L_IRQ_I2C2_EV);
	task_enable_irq(STM32L_IRQ_I2C2_ER);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(i2cinit, i2c_init2);

int i2c_init(void)
{
	return EC_SUCCESS;
}
