/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART module for ISH 3.0 */
#include <stdint.h>
#include <stddef.h>
#include "uart_defs.h"
#include "atomic.h"
#include "task.h"
#include "common.h"
#include "registers.h"
#include "uart.h"
#include "uart_defs.h"

#define TX_FIFO_SIZE 16

static const uint32_t baud_conf[][BAUD_TABLE_MAX] = {
	{B9600, 9600},
	{B57600, 57600},
	{B115200, 115200},
	{B921600, 921600},
	{B2000000, 2000000},
	{B3000000, 3000000},
	{B3250000, 3250000},
	{B3500000, 3500000},
	{B4000000, 4000000},
	{B19200, 19200},
};

UART_CTX uart_ctx[UART_DEVICES];

static int init_done;

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_start(void)
{
}

void uart_tx_stop(void)
{
}

void uart_tx_flush(void)
{
}

int uart_tx_ready(void)
{
	return 0;
}

int uart_tx_in_progress(void)
{
	return 0;
}

int uart_rx_available(void)
{
	return 0;
}

void uart_write_char(char c)
{
	int id = 1;

	while ((REG8(LSR(id)) & LSR_TEMT) == 0)
		;

	REG8(THR(id)) = c;
}

int uart_read_char(void)
{
	return 0;
}

void uart_disable_interrupt(void)
{
}

void uart_enable_interrupt(void)
{
}

/**
 * Interrupt handler for UART
 */
void uart_ec_interrupt(void)
{
}

#ifdef CONFIG_LOW_POWER_IDLE
void uart_enter_dsleep(void)
{
}


void uart_exit_dsleep(void)
{
}

void uart_deepsleep_interrupt(enum gpio_signal signal)
{
}
#endif /* CONFIG_LOW_POWER_IDLE */


static void uart_start_transaction(void)
{
	int i;

	for (i = 0; i < UART_DEVICES; i++) {
		if ((uart_ctx[i].uart_state & UART_STATE_CG) == 0)
			return;
	}
}

static int uart_return_baud_rate_by_id(int baud_rate_id)
{
	int i;

	for (i = 0; i < (sizeof(baud_conf) / sizeof(baud_conf[0])); i++) {
		if (baud_conf[i][BAUD_IDX] == baud_rate_id)
			return baud_conf[i][BAUD_SPEED];
	}
	return -1;
}


static void uart_hw_init(UART_PORT id)
{
	uint32_t divisor;	/* baud rate divisor */
	uint8_t mcr = 0;
	uint8_t fcr = 0;
	UART_CTX *ctx = &uart_ctx[id];

	interrupt_disable();
	/* calculate baud rate divisor */
	divisor = (ctx->input_freq / ctx->baud_rate) >> 4;

	REG32(MUL(ctx->id)) = (divisor * ctx->baud_rate);
	REG32(DIV(ctx->id)) = (ctx->input_freq / 16);
	REG32(PS(ctx->id)) = 16;

	/* set the DLAB to access the baud rate divisor registers */
	REG8(LCR(ctx->id)) = LCR_DLAB;
	REG8(DLL(ctx->id)) = (divisor & 0xff);
	REG8(DLH(ctx->id)) = ((divisor >> 8) & 0xff);

	/* 8 data bits, 1 stop bit, no parity, clear DLAB */
	REG8(LCR(ctx->id)) = LCR_8BIT_CHR;

	if (ctx->client_flags & UART_CONFIG_HW_FLOW_CONTROL)
		mcr = MCR_AUTO_FLOW_EN;
	mcr |= MCR_INTR_ENABLE;	/* needs to be set regardless of flow control */

	mcr |= (MCR_RTS | MCR_DTR);
	REG8(MCR(ctx->id)) = mcr;

	fcr = FCR_FIFO_SIZE_64 | FCR_ITL_FIFO_64_BYTES_1;

	/* configure FIFOs */
	REG8(FCR(ctx->id)) = (fcr | FCR_FIFO_ENABLE
				| FCR_RESET_RX | FCR_RESET_TX);

	/* enable UART unit */
	REG32(ABR(ctx->id)) = ABR_UUE;

	/* clear the port */
	REG8(RBR(ctx->id));

	REG8(IER(ctx->id)) = 0x00;

	interrupt_enable();
}

static void uart_stop_hw(UART_PORT id)
{
	int i;

	/* manually clearing the fifo from possible noise.
	 * entering d0i3 when fifo is not cleared may result in a hang.*/
	uint32_t fifo_len =
	    (REG32(FOR(id)) & FOR_OCCUPANCY_MASK) >> FOR_OCCUPANCY_OFFS;
	for (i = 0; i < fifo_len; i++)
		(void)REG8(RBR(id));

	/* no interrupts are enabled */
	REG8(IER(id)) = 0;
	REG8(MCR(id)) = 0;

	/* clear and disable FIFOs */
	REG8(FCR(id)) = (FCR_RESET_RX | FCR_RESET_TX);

	/*  disable uart unit */
	REG32(ABR(id)) = 0;
}

int uart_client_init(UART_PORT id, uart_info_t *uart_info,
		     uint32_t baud_rate_id, int flags)
{
	if (uart_ctx[id].base == 0)
		return UART_ERROR;

	if (id >= UART_DEVICES)
		return UART_ERROR;
	if (!bool_compare_and_swap_u32(&uart_ctx[id].is_open, 0, 1))
		return UART_BUSY;

	uart_ctx[id].rx_free_index = 0;
	uart_ctx[id].rx_used_index = 0;
	uart_ctx[id].rx_last_index = 0;
	uart_ctx[id].tx_first_index = 0;
	uart_ctx[id].tx_send_index = 0;
	uart_ctx[id].tx_last_index = 0;
	uart_ctx[id].tx_buf_offset = 0;

	uart_ctx[id].uart_info = uart_info;
	uart_ctx[id].baud_rate = uart_return_baud_rate_by_id(baud_rate_id);

	if ((uart_ctx[id].baud_rate == -1) || (uart_ctx[id].baud_rate == 0))
		uart_ctx[id].baud_rate = UART_DEFAULT_BAUD_RATE;
	uart_ctx[id].client_flags = flags;

	uart_start_transaction();
	atomic_and(&uart_ctx[id].uart_state, ~UART_STATE_CG);
	uart_hw_init(id);

	return EC_SUCCESS;
}

void uart_drv_init(void)
{
	int i;
	uint32_t fifo_len;

	uart_ctx[0].base = UART0_BASE;
	uart_ctx[1].base = UART1_BASE;

	for (i = 0; i < UART_DEVICES; i++) {
		uart_ctx[i].input_freq = UART_ISH_INPUT_FREQ;
		uart_ctx[i].addr_interval = UART_ISH_ADDR_INTERVAL;
		uart_ctx[i].id = i;
		uart_ctx[i].uart_state = UART_STATE_CG;

		/* disable UART */
		uart_stop_hw(i);
	}

	/* enable HSU global interrupts (DMA/U0/U1) and set PMEN bit
	 * to allow PMU to clock gate ISH */
	REG32(HSU_BASE + HSU_REG_GIEN) = (
			GIEN_DMA_EN
			| GIEN_UART0_EN
			| GIEN_UART1_EN
			| GIEN_PWR_MGMT);

	/* There is a by design HW "bug" where all UARTs are enabled by default
	 * but they must be disbled to enter clock gating.
	 * UART0 and UART1 are disabled during their init - but we don't init
	 * UART2 so as a w/a we disable UART2 even though it isn't being used.
	 * we also clear UART 2 fifo, which may cause problem entrying TCG is
	 * not empty (we do the same for UART0 and 1 in "uart_stop_hw" */

	fifo_len = (REG32(UART2_BASE + UART_REG_FOR)
		& FOR_OCCUPANCY_MASK) >> FOR_OCCUPANCY_OFFS;

	for (i = 0; i < fifo_len; i++)
		(void)REG8((UART2_BASE + UART_REG_RBR));

	REG32(UART2_BASE + UART_REG_ABR) = 0;

	task_enable_irq(ISH30_UART0_IRQ);
	task_enable_irq(ISH30_UART1_IRQ);
}

void uart_init(void)
{
	uart_info_t uart_info;

	uart_info.event_flag_rx = 0;
	uart_info.event_flag_tx = 0;

	uart_drv_init();

	uart_client_init(UART_PORT_1, &uart_info, B115200, 0);
	init_done = 1;
}
