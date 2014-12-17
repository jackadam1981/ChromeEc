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

/* SPI Flash Max transfer size */
#define GC_SPI_FLASH_MAX_XFER_SIZE       (1<<7)

/* SPI Flash Max transfer mask */
#define GC_SPI_FLASH_MAX_XFER_MASK       (GC_SPI_FLASH_MAX_XFER_SIZE - 1)

static int debug;
static struct fifo128 g_spi_tx_submission_queue;
static struct fifo128 g_spi_tx_completion_queue;

/* SPI Statistic Counters */
static uint32_t spi_sts_tx_count;

static uint32_t g_tx_data[SPS_FIFO_CMD_SIZE>>2];
static uint32_t g_rx_data[SPS_FIFO_CMD_SIZE>>2];

/*
 * Set SPI transaction length
 * This is useful for performing reads without having to
 * pre-fill the TX buffers. Len should be <= GC_SPI_FLASH_MAX_XFER_SIZE.
 */
static void spi_length_set(uint32_t inst, uint32_t len)
{
	len -= 1;
	len &= GC_SPI_FLASH_MAX_XFER_MASK;
	GWRITE_FIELD_I(SPI, inst, XACT, SIZE, len);
}

/*
 * Copy data to the SPI TX buffer
 * RevA1: Requried input data is 4-byte aligned; len is multiple of 4
 * @param data Pointer to 32-bit data
 * @param len Length of data
 * @param tx_id ID of transmission
 */
static int spi_write32(uint32_t inst,
		const uint32_t *data, uint32_t len, int tx_id)
{
	int i;
	volatile uint32_t *dest;

	if (len & 0x3)
		return EC_ERROR_PARAM1;

	dest = GREG32_ADDR_I(SPI, inst, TX_DATA);

	if (debug)
		CPRINTS("spi wr [%X] <-- [%X] len:%d", dest, data, len);
	spi_length_set(inst, len);

	len >>= 2;
	for (i = 0; i < len; i++)
		dest[i] = data[i];

	fifo128_enque8(&g_spi_tx_submission_queue, tx_id);

	GWRITE_FIELD_I(SPI, inst, XACT, START, 1);
	return EC_SUCCESS;
}

/*
 * Read data from the SPI RX buffer
 * RevA1: Requried input data is 4-byte aligned; len is multiple of 4
 * @param data Pointer to 32-bit data
 * @param len Length of data
 */
static int spi_read32(uint32_t inst, uint32_t *data, uint32_t len)
{
	int i;
	volatile uint32_t *src;

	if (len & 0x3)
		return EC_ERROR_PARAM1;

	src = GREG32_ADDR_I(SPI, inst, RX_DATA);
	if (debug)
		CPRINTS("spi rd [%X] --> [%X] len:%d", src, data, len);
	len >>= 2;
	for (i = 0; i < len; i++)
		data[i] = src[i];

	return EC_SUCCESS;
}

/*
 * Configure SPI data transmission format
 */
void spi_configure(enum spi_clock_mode clk_mode)
{
	/* Configure SPI master */
	GWRITE_FIELD(SPI, CTRL, CPHA, clk_mode & 1);
	GWRITE_FIELD(SPI, CTRL, CPOL, (clk_mode >> 1) & 1);

	/* [5:2] CSB to SCK setup time in SCK cycles + 1.5 */
	GWRITE_FIELD(SPI, CTRL, CSBSU, 1);
	/* [9:6] CSB from SCK hold time in SCK cycles + 1 */
	GWRITE_FIELD(SPI, CTRL, CSBHLD, 1);

	/* [21:10] SPI clk divider */
	GWRITE_FIELD(SPI, CTRL, IDIV,    7);

	GWRITE_FIELD(SPI, CTRL, TXBITOR, BIT_ORDER_MSB_FIRST);
	GWRITE_FIELD(SPI, CTRL, RXBITOR, BIT_ORDER_MSB_FIRST);

	/*Tx Interrupt enable */
	GREG32(SPI, ICTRL) = 1;
}


/*
 * TBD: Tx Interrupt enable
 *  GREG32(SPI, ICTRL) = 1;  enable tx_done interrupt
 *  GREG32(SPI, ISTATE) == 1; check if tx_done
 *  GREG32(SPI, ISTATE_CLR) = 1; write 1 to clear tx_done
 */
int spi_enable(int enable)
{
	static uint8_t enable_flag;

	if (enable == enable_flag)
		return EC_SUCCESS;

	if (enable)
		spi_configure(0);

	enable_flag = enable;
	return EC_SUCCESS;
}


int spi_transaction32(const uint32_t *txdata, int txlen,
		    uint32_t *rxdata, int rxlen)
{
	int rc = EC_SUCCESS;

	uint32_t inst = 0;
	uint8_t tx_id, rx_id;

	tx_id = 0xee;
	rx_id = 0x0;
	if (txdata && (txlen > 0) && !(txlen & 0x3))
		rc = spi_write32(inst, txdata, txlen, tx_id);

	if (rc)
		return rc;

	if (rxdata && (rxlen > 0) && !(rxlen & 0x3)) {
		while (0 == fifo128_get_size(&g_spi_tx_completion_queue)) {
			usleep(10);
			watchdog_reload();
		}
		fifo128_deque8(&g_spi_tx_completion_queue, &rx_id);
		if (rx_id != tx_id)
			CPRINTS("rx_id:%d != tx_id:%d\n", rx_id, tx_id);
		spi_read32(inst, rxdata, rxlen);
	}

	return EC_SUCCESS;
}

static void spi_init(void)
{
	/* init clock */
	pmu_clock_en(PERIPH_SPI);

	spi_enable(1);

	fifo128_init(&g_spi_tx_submission_queue);
	fifo128_init(&g_spi_tx_completion_queue);

	task_enable_irq(GC_IRQNUM_SPI0_SPITXINT);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);


static void spi_tx_interrupt(int port)
{
	uint8_t tx_id = 0;
	fifo128_deque8(&g_spi_tx_submission_queue, &tx_id);
	fifo128_enque8(&g_spi_tx_completion_queue, tx_id);

	if (fifo128_is_empty(&g_spi_tx_submission_queue))
		GWRITE_FIELD_I(SPI, port, ISTATE_CLR, TXDONE, 1);
	else {
		GWRITE_FIELD_I(SPI, port, ISTATE_CLR, TXDONE, 1);
		GWRITE_FIELD_I(SPI, port, XACT, START, 1);
	}

	spi_sts_tx_count++;
}

void _spi0_tx_interrupt(void)
{
	spi_tx_interrupt(0);
}
DECLARE_IRQ(GC_IRQNUM_SPI0_SPITXINT, _spi0_tx_interrupt, 1);

static int spi_loopback_test(int val, int num)
{
	int rc = 0, i, c;
	uint8_t tx_len = SPS_FIFO_CMD_SIZE;
	uint8_t rx_len = SPS_FIFO_CMD_SIZE;

	CPRINTS("Loopback Test: num=%d (0x%p 0x%p)",
		num, g_tx_data, g_rx_data);

	CPRINTS("spi tx cnt:%d", spi_sts_tx_count);
	for (i = 0; i < num; i++) {

		for (c = 0; c < SPS_FIFO_CMD_SIZE/4; c++) {
			g_tx_data[c] = (val == 0xad) ? (val + c) : val;
			g_rx_data[c] = 0;
		}

		rc = spi_transaction32(g_tx_data, tx_len, g_rx_data, rx_len);
		if (rc) {
			CPRINTS("spi error; rc:%d\n", rc);
			cflush();
			return rc;
		}

		rc = spi_transaction32(g_tx_data, tx_len, g_rx_data, rx_len);
		if (rc) {
			CPRINTS("spi error; rc:%d\n", rc);
			cflush();
			return rc;
		}

		if (debug)
			CPRINTS("spi tx cnt:%d", spi_sts_tx_count);

		rc = memcmp(g_tx_data, g_rx_data, rx_len);
		if (rc) {
			CPRINTS("Loopback Test Failed.");
			for (i = 0; i < SPS_FIFO_CMD_SIZE/4; i++)
				CPRINTS("%08X %08X",
				g_tx_data[i], g_rx_data[i]);
			break;
		}
		watchdog_reload();
	}
	CPRINTS("spi tx cnt:%d", spi_sts_tx_count);
	if (!rc)
		CPRINTS("SPI->SPS Loopback Test Passed.");
	return rc;
}


static int command_spitest(int argc, char **argv)
{
	char *e;
	uint32_t seed = 0xfa, num = 0;

	if (argc < 1)
		return EC_ERROR_PARAM_COUNT;

	if (argc >= 2)
		num = strtoi(argv[1], &e, 0);

	if (argc >= 3)
		seed = strtoi(argv[2], &e, 0);

	spi_loopback_test(seed, num);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spitest, command_spitest,
			"[num] [seed]",
			"num: num of bytes; seed: a byte seed value.",
			NULL);
