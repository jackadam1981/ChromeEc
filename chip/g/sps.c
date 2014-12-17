/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fifo128.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "pmu.h"
#include "watchdog.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define BIT_ORDER_MSB_FIRST 1
#define BIT_ORDER_LSB_FIRST 0

/* SPS Hardware TX & RX FIFO Base Address and size */
#define SPS_FIFO_SIZE 0x400
#define SPS_TX_FIFO_BASE_ADDR (GBASE(SPS) + 0x1000)
#define SPS_RX_FIFO_BASE_ADDR (SPS_TX_FIFO_BASE_ADDR + SPS_FIFO_SIZE)

/* SPS Control Mode */
enum sps_mode {
	SPS_GENERIC_MODE = 0,
	SPS_SWETLAND_MODE = 1,
	SPS_ROM_MODE = 2,
	SPS_UNDEF_MODE = 3,
};

/* RevA1: only support 4-byte access; does not support byte access */
static int sps_fifo_need_workaround;

static struct fifo128 g_sps_tx_fifo;
static struct fifo128 g_sps_rx_fifo;

/* SPS Statistic Counters */
static uint32_t sps_sts_tx_count;
static uint32_t sps_sts_rx_count;

/*
 * Push data to the SPS TX FIFO
 * @param data uint32_t
 * @return push count: sizeof(uint32_t) or 0
 */
static int sps_push32(uint32_t inst, const uint32_t d32)
{
	volatile uint32_t *sps_base =
		(volatile uint32_t *)SPS_TX_FIFO_BASE_ADDR;
	uint32_t offset;
	uint32_t woff;

	offset = GREG32_I(SPS, inst, TXFIFO_WPTR);
	woff = (offset & 0x3FF) >> 2;
	sps_base[woff] = d32;

	GREG32_I(SPS, inst, TXFIFO_WPTR) = (offset+sizeof(uint32_t)) & 0x7FF;

	return sizeof(uint32_t);
}

/** Peek data32 from SPS RX FIFO
 *
 *  @returns
 *    the data in the receive buffer
 */
static volatile uint32_t *sps_rx_top32(uint32_t inst)
{
	volatile int32_t *sps_base =
		(volatile uint32_t *)SPS_RX_FIFO_BASE_ADDR;
	uint32_t offset = GREG32_I(SPS, inst, RXFIFO_RPTR);

	return &sps_base[(offset & 0x3FF) >> 2];
}

/*
 * Pop data from the SPS RX FIFO
 * @param data Pointer to 32-bit data
 * @return pop count: sizeof(uint32_t) or 0
 */
static int sps_rx_pop32(uint32_t inst, uint32_t *data)
{
	*data = *sps_rx_top32(inst);
	GREG32_I(SPS, inst, RXFIFO_RPTR) += sizeof(uint32_t);
	GREG32_I(SPS, inst, RXFIFO_RPTR) &= 0x7FF;
	return sizeof(uint32_t);
}

/** Configure the data transmission format
 *
 *  @param mode Clock polarity and phase mode (0 - 3)
 *
 */
static void sps_configure(enum sps_mode mode, enum spi_clock_mode clk_mode)
{
	/* Disable All Interrupts */
	GREG32(SPS, ICTRL) = 0;

	GWRITE_FIELD(SPS, CTRL, MODE, mode);
	GWRITE_FIELD(SPS, CTRL, IDLE_LVL, 0);
	GWRITE_FIELD(SPS, CTRL, CPHA, clk_mode & 1);
	GWRITE_FIELD(SPS, CTRL, CPOL, (clk_mode >> 1) & 1);
	GWRITE_FIELD(SPS, CTRL, TXBITOR, BIT_ORDER_MSB_FIRST);
	GWRITE_FIELD(SPS, CTRL, RXBITOR, BIT_ORDER_MSB_FIRST);
	/* xfer 0xff when tx fifo is empty */
	GREG32(SPS, DUMMY_WORD) = 0xff;

	/* [5,4,3]           [2,1,0]
	 * RX{DIS, EN, RST} TX{DIS, EN, RST}
	 */
	GREG32(SPS, FIFO_CTRL) = 0x9;
	GWRITE_FIELD(SPS, FIFO_CTRL, TXFIFO_RST, 1);
	GWRITE_FIELD(SPS, FIFO_CTRL, RXFIFO_RST, 1);
	GWRITE_FIELD(SPS, FIFO_CTRL, TXFIFO_EN, 1);
	GWRITE_FIELD(SPS, FIFO_CTRL, RXFIFO_EN, 1);

	GWRITE_FIELD(SPS, ICTRL, TXFIFO_LVL, 0);
	GWRITE_FIELD(SPS, ICTRL, RXFIFO_LVL, 1);

	GREG32(SPS, RXFIFO_THRESHOLD) = SPS_FIFO_CMD_SIZE - 1;
	GREG32(SPS, TXFIFO_THRESHOLD) = SPS_FIFO_CMD_SIZE - 1;
}

int sps_enable(int enable, enum sps_mode mode)
{
	static uint8_t sps_enable_flag;

	if (enable == sps_enable_flag)
		return EC_SUCCESS;

	if (enable)
		sps_configure(mode, 0);

	sps_enable_flag = enable;
	return EC_SUCCESS;
}

void sps_init_fifo(void)
{
	int i;
	volatile uint32_t *sps_tx_base =
		(volatile uint32_t *)SPS_TX_FIFO_BASE_ADDR;
	volatile uint32_t *sps_rx_base =
		(volatile uint32_t *)SPS_RX_FIFO_BASE_ADDR;
	volatile uint8_t *rx = (volatile uint8_t *)SPS_RX_FIFO_BASE_ADDR;
	volatile uint8_t *tx = (volatile uint8_t *)SPS_TX_FIFO_BASE_ADDR;
	for (i = 0; i < SPS_FIFO_SIZE/sizeof(uint32_t); i++) {
		sps_tx_base[i] = 0;
		sps_rx_base[i] = 0;
	}

	sps_tx_base[0] = 0x12345678;
	sps_rx_base[0] = 0x12345678;
	CPRINTS("sps tx %08X", sps_tx_base[0]);
	CPRINTS("sps tx %08X", sps_rx_base[0]);
	tx[0] = 0xfa;
	rx[0] = 0xea;
	CPRINTS("sps tx %08X", sps_tx_base[0]);
	CPRINTS("sps tx %08X", sps_rx_base[0]);
	if ((tx[0] == tx[1]) || (rx[0] == rx[1])) {
		CPRINTS("SPS FIFO need software workaround.");
		sps_fifo_need_workaround = 1;
	}
	sps_tx_base[0] = 0;
	sps_rx_base[0] = 0;

	fifo128_init(&g_sps_tx_fifo);
	fifo128_init(&g_sps_rx_fifo);
}

static void sps_init(void)
{
	pmu_clock_en(PERIPH_SPS);
	sps_enable(1, SPS_GENERIC_MODE);
	sps_init_fifo();

	task_enable_irq(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR);
	task_enable_irq(GC_IRQNUM_SPS0_TXFIFO_LVL_INTR);
}
DECLARE_HOOK(HOOK_INIT, sps_init, HOOK_PRIO_DEFAULT);


/* Interrupt handler stuff */
static void sps_invoke_handler(uint32_t d32)
{
	fifo128_enque32(&g_sps_rx_fifo, d32);
	return;
}

static void sps_rx_interrupt(int port)
{
	uint32_t d32;
	int num;
	int cnt;

	cnt = GREAD_FIELD_I(SPS, port, ISTATE, RXFIFO_LVL)*SPS_FIFO_CMD_SIZE;
	if (cnt == 0)
		return;

	/* if software fifo is full, skip and wait for the next interrupt */
	if ((fifo128_get_size(&g_sps_rx_fifo) + cnt) > FIFO_SIZE_128)
		return;

	while (cnt > 0) {
		num = sps_rx_pop32(port, &d32);
		sps_invoke_handler(d32);
		sps_sts_rx_count += num;
		cnt -= num;
	}
	GWRITE_FIELD(SPS, ICTRL, TXFIFO_LVL, 1);

	return;
}

static void sps_tx_interrupt(int port)
{
	uint32_t d32 = 0xFF;
	int num;
	int cnt;

	cnt = GREAD_FIELD_I(SPS, port, ISTATE, TXFIFO_LVL)*SPS_FIFO_CMD_SIZE;
	if (cnt == 0)
		return;

	num = fifo128_get_size(&g_sps_rx_fifo);
	if (num < cnt)
		return;

	while (cnt > 0) {
		fifo128_deque32(&g_sps_rx_fifo, &d32);
		num = sps_push32(port, d32);
		sps_sts_tx_count += num;
		cnt -= num;
	}
	GWRITE_FIELD(SPS, ICTRL, TXFIFO_LVL, 0);
}

void _sps0_rx_interrupt(void)
{
	sps_rx_interrupt(0);
}

void _sps0_tx_interrupt(void)
{
	sps_tx_interrupt(0);
}

DECLARE_IRQ(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR, _sps0_rx_interrupt, 1);
DECLARE_IRQ(GC_IRQNUM_SPS0_TXFIFO_LVL_INTR, _sps0_tx_interrupt, 1);

