/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SCP UART module */

#include "system.h"
#include "uart.h"
#include "uart_regs.h"
#include "util.h"

#define UARTN CONFIG_UART_CONSOLE
#define UART_IDLE_WAIT_US 500

static uint8_t init_done, tx_started;

void uart_init(void)
{
	const uint32_t baud_rate = CONFIG_UART_BAUD_RATE;
	/* TODO: use ULPOSC1 for S3 */
	const uint32_t uart_clock = 26000000;
	const uint32_t div = DIV_ROUND_NEAREST(uart_clock, baud_rate * 16);

#if UARTN == 0
	//REG32(0x60000000 + 0x1084) = (1 << 22);
#if 1
	SCP_UART_CK_SEL |= UART0_CK_SEL_VAL(UART_CK_SEL_26M);
	SCP_SET_CLK_CG |= CG_UART0_MCLK | CG_UART0_BCLK | CG_UART0_RST;

	while ((SCP_UART_CK_SEL & UART0_CK_SW_STATUS_MASK) !=
			UART_CK_SW_STATUS_26M << UART0_CK_SW_STATUS_SHIFT)
		;
#endif
#if 1
	/* set AP GPIO94 and GPIO95 to alt func 5 */
	AP_GPIO_MODE11_CLR = 0x77000000;
	AP_GPIO_MODE11_SET = 0x55000000;
#else
	/* set AP GPIO94 and GPIO95 to alt func 1 */
	AP_GPIO_MODE11_CLR = 0x77000000;
	AP_GPIO_MODE11_SET = 0x11000000;
#endif
#elif UARTN == 1
	SCP_UART_CK_SEL |= UART1_CK_SEL_VAL(UART_CK_SEL_26M);
	SCP_SET_CLK_CG |= CG_UART1_MCLK | CG_UART1_BCLK | CG_UART1_RST;
#endif
#if 0
	/* set AP GPIO164 and GPIO165 to alt func 3 */
	AP_GPIO_MODE20_CLR = 0x00770000;
	AP_GPIO_MODE20_SET = 0x00330000;
#endif

	/* Clear FIFO */
	UART_FCR(UARTN) = UART_FCR_ENABLE_FIFO
		| UART_FCR_CLEAR_RCVR
		| UART_FCR_CLEAR_XMIT;
	/* Line control: parity none, 8 bit, 1 stop bit */
	UART_LCR(UARTN) = UART_LCR_WLEN8;
	/* For baud rate <= 115200 */
	UART_HIGHSPEED(UARTN) = 0;

	/* DLAB start */
	UART_LCR(UARTN) |= UART_LCR_DLAB;
	UART_DLL(UARTN) = div & 0xff;
	UART_DLH(UARTN) = (div >> 8) & 0xff;
	UART_LCR(UARTN) &= ~UART_LCR_DLAB;
	/* DLAB end */

	/* Enable received data interrupt */
	UART_IER(UARTN) |= UART_IER_RDI;

#if (UARTN < SCP_UART_COUNT)
	//task_enable_irq(UART_TX_IRQ(UARTN));
	//task_enable_irq(UART_RX_IRQ(UARTN));
	/* UART RX IRQ needs an extra enable, no such register in MT8192 SCP */
	//SCP_INTC_UART_RX_IRQ |= 1 << UARTN;
#endif

	//gpio_config_module(MODULE_UART, 1);
	init_done = 1;
}

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_flush(void)
{
	while (!(UART_LSR(UARTN) & UART_LSR_TEMT))
		;
}

int uart_tx_ready(void)
{
	return UART_LSR(UARTN) & UART_LSR_THRE;
}

int uart_rx_available(void)
{
	return UART_LSR(UARTN) & UART_LSR_DR;
}

void uart_write_char(char c)
{
	while (!uart_tx_ready())
		;

	UART_THR(UARTN) = c;
}

int uart_read_char(void)
{
	return UART_RBR(UARTN);
}

void uart_tx_start(void)
{
	tx_started = 1;
	uart_process_output();
	disable_sleep(SLEEP_MASK_UART);
	UART_IER(UARTN) |= UART_IER_THRI;
}

void uart_tx_stop(void)
{
	tx_started = 0;

	UART_IER(UARTN) &= ~UART_IER_THRI;
	enable_sleep(SLEEP_MASK_UART);
}

void uart_process(void)
{
	uart_process_input();
	uart_process_output();
}

#if (UARTN < SCP_UART_COUNT)
void uart_tx_interrupt(void)
{
	uint8_t ier;

	ccprints("%s", __func__);
	cflush();

	task_clear_pending_irq(UART_TX_IRQ(UARTN));
	uart_process();
	ier = UART_IER(UARTN);
	UART_IER(UARTN) = 0;
	UART_IER(UARTN) = ier;
}
DECLARE_IRQ(UART_TX_IRQ(UARTN), uart_tx_interrupt, 2);

void uart_rx_interrupt(void)
{
	uint8_t ier;

	ccprints("%s", __func__);
	cflush();

	task_clear_pending_irq(UART_RX_IRQ(UARTN));
	SCP_CORE0_INTC_UART_RX_IRQ(UARTN) = BIT(1);
	uart_process();
	ier = UART_IER(UARTN);
	UART_IER(UARTN) = 0;
	UART_IER(UARTN) = ier;
}
DECLARE_IRQ(UART_RX_IRQ(UARTN), uart_rx_interrupt, 2);
#endif

void uart_task(void)
{
#if (UARTN >= SCP_UART_COUNT)
	while (1) {
		if (uart_rx_available() || tx_started)
			uart_process();
		else
			task_wait_event(UART_IDLE_WAIT_US);
	}
#endif
}
