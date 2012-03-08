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

static int tx_byte_count;
#if 0
static int rx_byte_count;
#endif

static task_id_t task_waiting_on_port2;
#if 0
static task_id_t task_waiting_on_port[NUM_PORTS];
static task_id_t i2c_read_task[NUM_PORTS];
static task_id_t i2c_write_task[NUM_PORTS];
static struct mutex port_mutex[NUM_PORTS];
#endif

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

#if 0
static void wait_btf(void)
{
	/* TODO: Add timeouts and error checking for safety */
	while (!(STM32L_I2C_SR1(2) & (1 << 2)))
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

	tx_byte_count = 0;
	for (i = 0; i < len; i++) {
		tx_byte_count++;
		STM32L_I2C_DR(2) = data[i];
		wait_tx();
	}

	/*
	 * In master mode: Stop generation after current byte transfer or
	 * after the current Start condition is sent.
	 * In slave mode: Release SCL and SDA after the current byte transfer.
	 */
#if 0
	STM32L_I2C_DR(2) = data[len - 1];
	if (do_stop)
		STM32L_I2C_CR1(2) |= (1 << 9);
	uart_printf("byte[%d]: 0x%02x\n", len - 1, data[len - 1]);
#endif
	/* Note: TxE not set if NACK is received */

	return len;
}

static void i2c2_error_interrupt(void)
{
	uint32_t stat1;

	stat1 = STM32L_I2C_SR1(2);
#ifdef CONFIG_DEBUG
	if (stat1 & 1 << 10) {
		/* ACK failed (NACK); expected after final byte written
		 * (SW clears AF at that point) */
		uart_printf("%s: AF detected\n", __func__);
	}
	uart_printf("%s: tx byte count: %d\n", __func__, tx_byte_count);
#if 0
	uart_printf("%s: tx byte count: %d, rx_byte_count: %d\n",
			__func__, tx_byte_count, rx_byte_count);
#endif
	uart_printf("%s: I2C_SR1(2): 0x%08x\n", __func__, stat1);
	uart_printf("%s: I2C_SR2(2): 0x%08x\n", __func__, STM32L_I2C_SR2(2));
#endif
	stat1 &= ~0x0000df00;
	STM32L_I2C_SR1(2) &= stat1;
}
DECLARE_IRQ(STM32L_IRQ_I2C2_ER, i2c2_error_interrupt, 2);

void i2c_work_task(void)
{
	uint8_t dummy = 0xaa;
	task_waiting_on_port2 = task_get_current();

	while (1) {
		task_wait_msg(-1);

		/* RxE; AP is waiting for EC response */
		switch (i2c2_xmit_mode) {
		case MODE_NOOP:
			uart_printf("%s: sending dummy byte\n", __func__);
			i2c_write_raw(&dummy, 1, 0);
			break;
		case MODE_KBC:
			i2c_write_raw(kb_packet, kb_packet_len, 1);
			break;
		default:
			uart_printf("%s: unexpected mode %u\n",
					__func__, i2c2_xmit_mode);
			break;
		}
	}
}

static void i2c_handle_interrupt(int port)
{
	uint32_t stat1;

	/* save and clear status */
	stat1 = STM32L_I2C_SR1(2);
	STM32L_I2C_SR1(2) = 0;

	/* transfer matched our slave address */
	if (stat1 & (1 << 1)) {
		/* cleared by reading SR1 followed by reading SR2 */
		/* FIXME: do we need to do this again after ADDR is read the
		 * first time? */
		STM32L_I2C_SR1(2);
		STM32L_I2C_SR2(2);
#ifdef CONFIG_DEBUG
		uart_printf("%s: ADDR\n", __func__);
#endif
	} else if (stat1 & (1 << 2)) {
		;
#ifdef CONFIG_DEBUG
		uart_printf("%s: BTF\n", __func__);
#endif
	} else if (stat1 & (1 << 4)) {
		/* Clear STOPF bit by reading SR1 and then writing CR1 */
		STM32L_I2C_SR1(2);
		STM32L_I2C_CR1(2) = STM32L_I2C_CR1(2);
#ifdef CONFIG_DEBUG
		uart_printf("%s: STOPF\n", __func__);
#endif
	} else {
		uart_printf("%s: unknown event\n", __func__);
	}

	if (stat1 & (1 << 6)) {
		/* RxNE; AP issued write command */
		i2c2_xmit_mode = STM32L_I2C_DR(2);
#ifdef CONFIG_DEBUG
		uart_printf("%s: i2c2_xmit_mode: %02x\n",
				__func__, i2c2_xmit_mode);
#endif
	} else if (stat1 & (1 << 7)) {
		task_send_msg(TASK_ID_I2C_WORK, TASK_ID_I2C_WORK, 0);
	}
}
static void i2c2_interrupt(void) { i2c_handle_interrupt(2); }
DECLARE_IRQ(STM32L_IRQ_I2C2_EV, i2c2_interrupt, 3);

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
	int rc = 0;

	/* FIXME: Add #defines to determine which channels to init */
	rc |= i2c_init2();
	return rc;
}
