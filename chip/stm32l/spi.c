/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI module for Chrome EC.
 * This is the stupid polling version.
 */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "registers.h"
#include "spi.h"
#include "uart.h"
#include "util.h"

#define NUM_PORTS 2
#define SPI1      STM32L_SPI1_PORT
#define SPI2      STM32L_SPI2_PORT

#define MAX_TRIES 1000

/*
 * Notes:
 *
 * Ports configured to shift each byte of data MSB first.
 *
 */

static task_id_t task_waiting_on_port[NUM_PORTS];
static uint16_t spi_sr[NUM_PORTS];



#if 0
static int wait_rx(uint32_t port)
{
	uint32_t base = stm32l_spi_addr(port);

	/* FIXME: superfluous debug print */
	uart_printf("%s: rxne: %d\n", __func__, rxne(base));
	while (!rxne(base)) {
		task_waiting_on_port[port] = task_get_current();
		STM32L_SPI_CR2(base) |= 1 << 6;
		wait_msg = task_wait_msg(1);
		STM32L_SPI_CR2(base) &= ~(1 << 6);
		task_waiting_on_port[port] = -1;
		if (wait_msg == 1 << TASK_ID_TIMER)
			return EC_ERROR_TIMEOUT;
	}
	/* FIXME: superfluous debug print */
	uart_printf("%s: rxne: %d\n", __func__, rxne(base));

	/* Check for errors */
	if (STM32L_SPI_SR(base) & 0x60)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

static int wait_tx(int port)
{
	uint32_t base = stm32l_spi_addr(port);

	/* FIXME: extra debug stuff */
	uart_printf("[%s()] base: 0x%08x\n", __func__, base);
	while (!txe(base)) {
		uart_printf("[%s()] txe not empty, waiting...\n", __func__);

		/* enable TXE interrupt and wait for it */
		task_waiting_on_port[port] = task_get_current();
		STM32L_SPI_CR2(base) |= 1 << 7;
		wait_msg = task_wait_msg(100000);
		STM32L_SPI_CR2(base) &= ~(1 << 7);
		task_waiting_on_port[port] = -1;
		if (wait_msg == 1 << TASK_ID_TIMER)
			return EC_ERROR_TIMEOUT;
	}

	uart_printf("[%s()] txe empty\n", __func__);
	while (1)
		udelay(1000000);
	/* Check for errors */
	if (STM32L_SPI_SR(base) & 0x60)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}
#endif
#if 0
static int rxne(int port)
{
	return (STM32L_SPI_SR(port) & 0x01) ? 1 : 0;
}

static int txe(int port)
{
	return (STM32L_SPI_SR(port) & 0x02) ? 1 : 0;
}
#endif

static int wait_rx(uint32_t port)
{
	int tries = 0;
	int rc = 0;

#if 0
	while (!(x & 0x01)) {
		uart_printf("%s: status: 0x%02x\n", __func__, x);
		if (tries > MAX_TRIES) {
			uart_printf("exceeded max tries\n");
			break;
		}
		tries++;
		x = STM32L_SPI_SR(port);
	}
#endif
	while (!(STM32L_SPI_SR(port) & 0x01)) {
		if (tries > MAX_TRIES) {
			uart_printf("exceeded max tries\n");
			rc = 1;
			break;
		}
		tries++;
		usleep(100);
	}
//	uart_printf("%s: status: 0x%02x\n", __func__, x);
	return rc;
}

static void wait_tx(int port)
{
	while (!(STM32L_SPI_SR(port) & 0x02))
		;
}

int spi_read(int port, void *buf, int len)
{
	unsigned int i;
	int rc = EC_SUCCESS;
	uint8_t *data = buf;

	/* FIXME: superfluous debug print */
	uart_printf("%s: reading %u bytes from port %d: ", __func__, len, port);
//	uart_printf("%s: CR1: 0x%02x\n", __func__, STM32L_SPI_CR1(port));
	for (i = 0; i < len; i++) {
//		if ((rc = spi_err(port))) {
//			rc = EC_ERROR_UNKNOWN; 
//			break;
//		}

		rc = wait_rx(port);
		if (rc)
			break;
		data[i] = STM32L_SPI_DR(port);
//		uart_printf("0x%02x ", data[i]);
	}
//	uart_printf("\n");

	/* FIXME: superfluous debug print */
//	uart_printf("%s: finished reading bytes\n", __func__);

	return rc;
}


int spi_write(int port, void *buf, int len)
{
	int i;
	int rc = EC_SUCCESS;
	uint8_t *data = buf;

	/* FIXME: extra debug print */
	uart_printf("[%s()] writing %u bytes to port %d\n", __func__, len, port);

#if 0
	/* wait for any remaining data to clear out */
	if (wait_tx(port))
		return EC_ERROR_UNKNOWN;
	/* FIXME: extra debug stuff */
	uart_printf("[%s()] checkpoint...\n", __func__);
	while(1)
		usleep(1000000);

#endif
//	uart_printf("%s: writing data: ", __func__);
	for (i = 0; i < len; i++) {

//		if ((i % 8 == 0) && (i != 0))
//			uart_printf("\n");
//		uart_printf("0x%02x ", data[i]);

		STM32L_SPI_DR(port) = data[i];
		wait_tx(port);
//		if ((rc = spi_err(port)))
//			break;

//		if (i % 8 == 0)
//			uart_printf("\n");
	}
//	uart_printf("\n");
//	uart_printf("%s: wrote %d bytes\n", __func__, i);
	return rc;
}

uint8_t spi1_buf[16];
void spi1_work_task(void)
{
	while (1) {
		task_wait_msg(-1);
		
		spi_sr[SPI1] = STM32L_SPI_SR(SPI1);

		if (spi_sr[SPI1] & (1 << 6)) {
			uart_printf("%s: overrun detected\n", __func__);
		} else if (spi_sr[SPI1] & (1 << 5)) {
			uart_printf("%s: mode fault detected\n", __func__);
		} else if (spi_sr[SPI1] & (1 << 3)) {
			uart_printf("%s: underrun detected\n", __func__);
		} else if (spi_sr[SPI1] & (1 << 0)) {
			spi_read(SPI1, &spi1_buf[0], 1);
			uart_printf("%s: echoing %02x\n", __func__, spi1_buf[0]);
			spi_write(SPI1, &spi1_buf[0], 1);
		}
	}
}

/* Handles an interrupt on the specified port. */
static void spi_interrupt(int port)
{
	int id = task_waiting_on_port[port];

	uart_printf("[%s()] caught interrupt\n", __func__);

	/* Wake up the task which was waiting on the interrupt, if any */
	if (id != TASK_ID_INVALID)
		task_send_msg(id, id, 0);
}


static void spi1_interrupt(void) { spi_interrupt(SPI1); };
//static void spi2_interrupt(void) { spi_interrupt(SPI2); };

DECLARE_IRQ(STM32L_IRQ_SPI1, spi1_interrupt, 2);
//DECLARE_IRQ(STM32L_IRQ_SPI2, spi2_interrupt, 2);

int spi_init(int argc, char **argv)
{
	int i;

	uart_printf("[%s()] initializing SPI...\n", __func__);

	/* No tasks are waiting on ports */
	for (i = 0; i < NUM_PORTS; i++)
		task_waiting_on_port[i] = TASK_ID_INVALID;

#if defined(BOARD_discovery) || defined(BOARD_daisy)
	/**
	 * SPI1
	 * PA7: SPI1_MOSI
	 * PA6: SPI1_MISO
	 * PA5: SPI1_SCK
	 * PA4: SPI1_NSS
	 *
	 * 8-bit data, master mode, full-duplex, clock is fpclk / 2
	 */

	/* slave mode (default) */
	//STM32L_SPI_CR1(1) &= ~(1 << 2);

	/* BRR[2:0] (0b011 = fpclk / 16) */
//	STM32L_SPI_CR1(1) &= ~0x0038;	
//	STM32L_SPI_CR1(1) |= 0x3 << 3;

#else
#error "Need to know how to set up SPI for this board"
#endif

	/* enable RxNE, and error interrupts */
	//STM32L_SPI_CR2(SPI1) |= (1 << 7) | (1 << 6) | (1 << 5);
	STM32L_SPI_CR2(SPI1) |= (1 << 6) | (1 << 5);

	task_enable_irq(STM32L_IRQ_SPI1);

	/* peripheral enable */
	STM32L_SPI_CR1(SPI1) |= (1 << 6);

	/* FIXME: superfluous debug print */
	uart_printf("[%s()] CR1: 0x%04x, CR2: 0x%04x\n",
	            __func__, STM32L_SPI_CR1(SPI1), STM32L_SPI_CR2(SPI1));
	uart_printf("[%s()] done.\n", __func__);
	return EC_SUCCESS;
}
