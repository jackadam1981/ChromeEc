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

/* SPS Hardware TX & RX FIFO Base Address and size */
#define SPS_FIFO_SIZE 0x400
#define SPS_TX_FIFO_BASE_ADDR (GBASE(SPS) + 0x1000)
#define SPS_RX_FIFO_BASE_ADDR (SPS_TX_FIFO_BASE_ADDR + SPS_FIFO_SIZE)

/*
 * SPS Command size:
 * 64 means got 1 SPS FIFO LVL interrupt per 64 byte received.
 */
#define SPS_FIFO_CMD_SIZE 64

/*
 * SPS SW FIFO
 */
#define SPS_SW_FIFO_SIZE GC_SPI_FLASH_MAX_XFER_SIZE
#define SPS_SW_FIFO_MASK (SPS_SW_FIFO_SIZE-1)

/*
 * SPI Clock polarity and phase mode (0 - 3)
 * @code
 * clk mode | POL PHA
 * ---------+--------
 *   0      |  0   0
 *   1      |  0   1
 *   2      |  1   0
 *   3      |  1   1
 * ---------+--------
 * @endcode
 */
enum spi_clock_mode {
	SPI_CLOCK_MODE0 = 0,
	SPI_CLOCK_MODE1 = 1,
	SPI_CLOCK_MODE2 = 2,
	SPI_CLOCK_MODE3 = 3
};

/* SPS Control Mode */
enum sps_mode {
	SPS_GENERIC_MODE = 0,
	SPS_SWETLAND_MODE = 1,
	SPS_ROM_MODE = 2,
	SPS_UNDEF_MODE = 3,
};

/* RevA1: only support 4-byte access; does not support byte access */
static int sps_fifo_need_workaround;
static int debug;

static struct fifo128 g_sps_tx_fifo;
static struct fifo128 g_sps_rx_fifo;

static struct fifo128 g_spi_tx_submission_queue;
static struct fifo128 g_spi_tx_completion_queue;

/* SPI/SPS Statistic Counters */
static uint32_t spi_sts_tx_count;

static uint32_t sps_sts_tx_count;
static uint32_t sps_sts_rx_count;

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

	task_enable_irq(GC_IRQNUM_SPI0_SPITXINT);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);


/*
 * SPI Slave Interface
 */
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

	fifo128_init(&g_spi_tx_submission_queue);
	fifo128_init(&g_spi_tx_completion_queue);
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

void _sps0_rx_interrupt(void)
{
	sps_rx_interrupt(0);
}
void _sps0_tx_interrupt(void)
{
	sps_tx_interrupt(0);
}
void _spi0_tx_interrupt(void)
{
	spi_tx_interrupt(0);
}
DECLARE_IRQ(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR, _sps0_rx_interrupt, 1);
DECLARE_IRQ(GC_IRQNUM_SPS0_TXFIFO_LVL_INTR, _sps0_tx_interrupt, 1);
DECLARE_IRQ(GC_IRQNUM_SPI0_SPITXINT, _spi0_tx_interrupt, 1);

static int spi_sps_loopback_test(int val, int num)
{
	int rc = 0, i, c;
	uint8_t tx_len = SPS_FIFO_CMD_SIZE;
	uint8_t rx_len = SPS_FIFO_CMD_SIZE;

	CPRINTS("SPS FIFO BASE: 0x%08X, 0x%08X num:%d (0x%p 0x%p)",
		SPS_TX_FIFO_BASE_ADDR, SPS_RX_FIFO_BASE_ADDR, num,
		g_tx_data, g_rx_data);

	CPRINTS("spi tx cnt:%d", spi_sts_tx_count);
	CPRINTS("sps tx cnt:%d", sps_sts_tx_count);
	CPRINTS("sps rx cnt:%d", sps_sts_rx_count);

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

		if (debug) {
			CPRINTS("spi tx cnt:%d", spi_sts_tx_count);
			CPRINTS("sps tx cnt:%d", sps_sts_tx_count);
			CPRINTS("sps rx cnt:%d", sps_sts_rx_count);
		}

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
	CPRINTS("sps tx cnt:%d", sps_sts_tx_count);
	CPRINTS("sps rx cnt:%d", sps_sts_rx_count);
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

	spi_sps_loopback_test(seed, num);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spitest, command_spitest,
			"[num] [seed]",
			"num: num of bytes; seed: a byte seed value.",
			NULL);

static int command_spi(int argc, char **argv)
{
	ccprintf("rx count %d, tx count %d\n",
		 sps_sts_rx_count, sps_sts_tx_count);
	/*
	fifo128_print32(&g_sps_rx_fifo);
	*/
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(spi, command_spi, "", "", NULL);
