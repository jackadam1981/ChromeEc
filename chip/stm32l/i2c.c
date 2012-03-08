/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "board.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "registers.h"
#include "timer.h"	/* FIXME: remove this */
#include "task.h"
#include "uart.h"

/* 8-bit I2C slave address */
#define I2C_ADDRESS 0xEC

/* I2C bus frequency */
#define I2C_FREQ 100000 /* Hz */

/* Clock divider for I2C controller */
#define I2C_CCR (CPU_CLOCK/(2 * I2C_FREQ))

#define NUM_PORTS 2
//static task_id_t task_waiting_on_port[NUM_PORTS];
static task_id_t task_waiting_on_port2;
//static task_id_t i2c_read_task[NUM_PORTS];
//static task_id_t i2c_write_task[NUM_PORTS];
//static struct mutex port_mutex[NUM_PORTS];

/* i2c_xmit_mode determines what EC sends (in between ACK and stop condition)
 * when AP initiates a read transaction */
enum i2c_xmit_mode {
	MODE_NOOP	= 0,	/* do nothing */
	MODE_KBC	= 1,	/* send keyboard state */
};

static enum i2c_xmit_mode i2c2_xmit_mode = MODE_NOOP;

#if 0
static void wait_rx(void)
{
	/* TODO: Add timeouts and error checking for safety */
	while (!(STM32L_I2C_SR1(2) & (1 << 6)))
		;
}
#endif

static void wait_tx(void)
{
	/* TODO: Add timeouts and error checking for safety */
	while (!(STM32L_I2C_SR1(2) & (1 << 7)))
		;
}

static int i2c_write_raw(void *buf, int len, int do_stop)
{
	int i;
	uint8_t *data = buf;

	for (i = 0; i < len - 1; i++) {
		STM32L_I2C_DR(2) = data[i];
		wait_tx();
	}

	/*
	 * In master mode: Stop generation after current byte transfer or
	 * after the current Start condition is sent.
	 * In slave mode: Release SCL and SDA after the current byte transfer.
	 */
	if (do_stop)
		STM32L_I2C_CR1(2) |= (1 << 9);

	STM32L_I2C_DR(2) = data[len];
	wait_tx();

	return len;
}

static void i2c2_error_interrupt(void)
{
//	task_disable_irq(STM32L_IRQ_I2C2_ER);
//	STM32L_I2C_CR2(2) &= ~(1 << 8);

	uart_printf("%s: I2C_SR1(2): 0x%04x\n", __func__, STM32L_I2C_SR1(2));
	uart_printf("%s: I2C_SR2(2): 0x%04x\n", __func__, STM32L_I2C_SR2(2));

//	STM32L_I2C_CR2(2) |= 1 << 8;
//	task_enable_irq(STM32L_IRQ_I2C2_ER);
}
DECLARE_IRQ(STM32L_IRQ_I2C2_ER, i2c2_error_interrupt, 2);

void i2c_work_task(void)
{
	uint32_t stat1;

	task_waiting_on_port2 = task_get_current();
	uart_printf("%s: task id: %d\n", __func__, (int)task_waiting_on_port2);

	while(1) {
		uart_printf("%s: waiting...\n", __func__);
		task_wait_msg(-1);
		uart_printf("%s: woke up\n", __func__);

		/* save and clear status */
		stat1 = STM32L_I2C_SR1(2);
		STM32L_I2C_SR1(2) = 0;

		/* transfer matched our slave address */
		if (stat1 & (1 << 1)) {
			STM32L_I2C_SR2(2);
			uart_printf("%s: address matched.\n", __func__);
		} else if (stat1 & (1 << 4)) {
			/* STOPF */
	//		STM32L_I2C_CR1(2) |= (1 << 9);
	//		uart_printf("%s: STOPF detected\n", __func__);
	//		task_send_msg(TASK_ID_I2C_WORK, TASK_ID_I2C_WORK, 0);
		} else {
			uart_printf("%s: address did not match, ignoring\n", __func__);
			continue;
		}

		if (stat1 & (1 << 6)) {
			/* RxNE; AP issued write command */
			i2c2_xmit_mode = STM32L_I2C_DR(2);
			uart_printf("%s: i2c2_xmit_mode: %02x\n",
			            __func__, i2c2_xmit_mode);
		} else if (stat1 & (1 << 7)) {
			uint8_t dummy = 0xaa;
			/* RxE; AP is waiting for EC response */
			switch (i2c2_xmit_mode) {
			case MODE_NOOP:
				/* FIXME: send a dummy byte for debugging */
				uart_printf("%s: sending dummy byte\n", __func__);
				i2c_write_raw(&dummy, 1, 0);
				break;
			case MODE_KBC:
				uart_printf("%s: sending keyboard state\n", __func__);
				i2c_write_raw(kb_packet, kb_packet_len, 1);
				break;
			default:
				uart_printf("%s: unexpected mode %u\n",
				            __func__, i2c2_xmit_mode);
				break;
			}
		}
	}
}

static void handle_interrupt(int port)
{
	int id = task_waiting_on_port2;

	uart_printf("%s: id: %d\n", __func__, id);
	if (id != TASK_ID_INVALID)
		task_send_msg(id, id, 0);
}
static void i2c2_interrupt(void) { handle_interrupt(2); }
DECLARE_IRQ(STM32L_IRQ_I2C2_EV, i2c2_interrupt, 2);

static int i2c_init2(void)
{
	uart_printf("%s: initializing i2c2...", __func__);
	/* enable I2C2 clock */
	STM32L_RCC_APB1ENR |= 1 << 22;

	/* set clock configuration : standard mode (100kHz) */
	STM32L_I2C_CCR(2) = I2C_CCR;

	/* set slave address */
	STM32L_I2C_OAR1(2) = I2C_ADDRESS;

	/* configuration : I2C mode / Periphal enabled, ACK enabled */
	STM32L_I2C_CR1(2) = (1 << 10) | (1 << 0);
	/* error and event interrupts enabled / input clock is 16Mhz */
	STM32L_I2C_CR2(2) = (1 << 9) | (1 << 8) | 0x10;

	/* clear status */
	STM32L_I2C_SR1(2) = 0;

	/* No tasks are waiting on ports */
	task_waiting_on_port2 = TASK_ID_INVALID;

	/* enable event and error interrupts */
	task_enable_irq(STM32L_IRQ_I2C2_EV);
	task_enable_irq(STM32L_IRQ_I2C2_ER);

	uart_printf("%s: CR1: 0x%04x, CR2: 0x%04x\n",
	            __func__, STM32L_I2C_CR1(2), STM32L_I2C_CR2(2));
	return EC_SUCCESS;
}

int i2c_init(void)
{
	int rc = EC_SUCCESS;	
	
	/* FIXME: Add #defines to determine which channels to init */
	if ((rc = i2c_init2()) != EC_SUCCESS)
		goto i2c_init_exit;

i2c_init_exit:
	return EC_SUCCESS;
}
