/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "pmu.h"

#define SPS_FIFO_BASE_ADDR (GBASE(SPS) + 0x1000)

static uint8_t g_data;

/* SPI Flash Max transfer size */
#define GC_SPI_FLASH_MAX_XFER_SIZE       128

/* SPI Flash Max transfer mask */
#define GC_SPI_FLASH_MAX_XFER_MASK       127

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

/*
 * Set SPI transaction length
 * This is useful for performing reads without having to
 * pre-fill the TX buffers. Len should be <= G_SPI_FLASH_MAX_SIZE.
 */
static void spi_length_set(uint32_t inst, uint32_t len)
{
	len -= 1;
	len &= GC_SPI_FLASH_MAX_XFER_MASK;
	GWRITE_FIELD_I(SPI, inst, XACT, SIZE, len);
}


/*
 * Start SPI transfer and wait for completion
 */
static int spi_start_wait(uint32_t inst)
{
	int cnt = 1000;
	GWRITE_FIELD_I(SPI, inst, ISTATE_CLR, TXDONE, 1);
	GWRITE_FIELD_I(SPI, inst, XACT, START, 1);
	while ((!GREAD_FIELD_I(SPI, inst, ISTATE_CLR, TXDONE)) && (--cnt > 0))
		usleep(100);
	return (cnt > 0) ? (EC_SUCCESS) : (-EC_ERROR_TIMEOUT);
}

/*
 * Copy data to the SPI TX buffer
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
static int spi_write(uint32_t inst, const uint8_t *data, uint32_t len)
{
	volatile uint32_t *dest;

	dest = GREG32_ADDR_I(SPI, inst, TX_DATA);

	CPRINTS("spi wr [%X] <-- [%X] len:%d", dest, data, len);

	spi_length_set(inst, len);
	memcpy((void *)dest, (void *)data, len);
	return spi_start_wait(inst);
}

/*
 * Read data from the SPI RX buffer
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
static void spi_read(uint32_t inst, uint8_t *data, uint32_t len)
{
	volatile uint32_t *src;
	src = GREG32_ADDR_I(SPI, inst, RX_DATA);
	CPRINTS("spi rd [%X] --> [%X] len:%d", src, data, len);
	memcpy((void *)data, (void *)src, len);
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

	if (enable) {
		gpio_config_module(MODULE_SPI_MASTER, 1);

		/* Configure SPI master */
		GWRITE_FIELD(SPI, CTRL, CPOL, 0);
		GWRITE_FIELD(SPI, CTRL, CPHA, 0);

		/* [5:2] CSB to SCK setup time in SCK cycles + 1.5 */
		GWRITE_FIELD(SPI, CTRL, CSBSU, 1);
		/* [9:6] CSB from SCK hold time in SCK cycles + 1 */
		GWRITE_FIELD(SPI, CTRL, CSBHLD, 1);

		/* [21:10] SPI clk divider */
		GWRITE_FIELD(SPI, CTRL, IDIV,    7);

		GWRITE_FIELD(SPI, CTRL, TXBITOR, 0); /* LSB first */
		GWRITE_FIELD(SPI, CTRL, RXBITOR, 0); /* LSB first */

		GREG32(SPI, ICTRL) = 0; /*Tx Interrupt disable */
	}
	/*
	 * TODO
	else {
	}
	*/
	enable_flag = enable;
	return EC_SUCCESS;
}

int spi_transaction(const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int rc = EC_SUCCESS;
	uint32_t inst = 0;

	if (txdata && (txlen > 0))
		rc = spi_write(inst, txdata, txlen);
	if (rc)
		return rc;

	if (rxdata && (rxlen > 0))
		spi_read(inst, rxdata, rxlen);

	return EC_SUCCESS;
}

static int spi_sps_loopback_test(void);
static void spi_init(void)
{
	/* init clock */
	CPRINTS("spi: init clk");
	pmu_clock_en(PERIPH_SPI);

	/* Ensure the SPI port is disabled.  This keeps us from interfering
	 * with the main chipset when we're not explicitly using the SPI
	 * bus. */
	spi_enable(1);

	/* loopback test */
	spi_sps_loopback_test();
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);


/*
 * SPI Slave Interface
 *--------------------
 */

#define sps_txfifo_empty(inst) GREAD_FIELD_I(SPS, inst, ISTATE, TXFIFO_EMPTY)
#define sps_txfifo_full(inst) GREAD_FIELD_I(SPS, inst, ISTATE, TXFIFO_FULL)
#define sps_txfifo_level(inst) GREAD_FIELD_I(SPS, inst, ISTATE, TXFIFO_LVL)
#define sps_rxfifo_level(inst) GREAD_FIELD_I(SPS, inst, ISTATE, RXFIFO_LVL)
#define sps_rxfifo_overflow(inst) \
	GREAD_FIELD_I(SPS, inst, ISTATE, RXFIFO_OVERFLOW)

/* SPS Control Mode */
enum sps_mode {
	SPS_GENERIC_MODE = 0,
	SPS_SWETLAND_MODE = 1,
	SPS_ROM_MODE = 2,
	SPS_UNDEF_MODE = 3,
};

/*
 * Push data to the SPS TX FIFO
 * @param data Pointer to 8-bit data
 * @return push count: 1 or 0
 */
static int sps_push(uint32_t inst, const uint8_t data)
{
	volatile uint8_t *sps_base = (volatile uint8_t *)SPS_FIFO_BASE_ADDR;
	uint32_t offset;
	if (sps_txfifo_full(inst))
		return 0;

	CPRINTS("sps push:%02X", data);
	usleep(10);

	offset = GREG32_I(SPS, inst, TXFIFO_WPTR);
	sps_base[offset] = data;
	GREG32_I(SPS, inst, TXFIFO_WPTR) = offset+1;
	CPRINTS("sps push:%02X", data);
	return 1;
}

/** Polls the SPI to see if data has been received
 *
 *  @returns
 *    0 if no data,
 *    n number of bytes received
 */
int sps_receive(uint32_t inst)
{
	return sps_rxfifo_level(inst);
}

/** Peek data from SPI RX FIFO
 *
 *  @returns
 *    the data in the receive buffer
 */
static uint8_t sps_top(uint32_t inst)
{
	volatile uint8_t *sps_base = (volatile uint8_t *)SPS_FIFO_BASE_ADDR;
	uint32_t offset = GREG32_I(SPS, inst, RXFIFO_RPTR);
	return sps_base[offset];
}

/*
 * Pop data from the SPI RX FIFO
 * @param data Pointer to 8-bit data
 * @return pop count: 1 or 0
 */
static int sps_pop(uint32_t inst, uint8_t *data)
{
	int cnt = sps_receive(inst);
	if (cnt == 0)
		return 0;

	*data = sps_top(inst);
	GREG32_I(SPS, inst, RXFIFO_RPTR) += 1;
	CPRINTS("sps pop:%02X", *data);
	return 1;
}

/*
 * Copy data to the SPS TX FIFO
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
static int sps_write(uint32_t inst, const uint8_t *data, uint32_t len)
{
	int i = 0, cnt, to = 0;
	while ((i < len) && (to++ < 10000)) {
		cnt = sps_push(inst, data[i]);
		i += cnt;
		usleep(5);
	}
	return i;
}

/*
 * Read data from the SPI RX FIFO
 * @param data Pointer to 8-bit data
 * @param len Length of data
 */
static int sps_read(uint32_t inst, uint8_t *data, uint32_t len)
{
	int i = 0, cnt, to = 0;
	while ((i < len) && (to++ < 10000)) {
		cnt = sps_pop(inst, &data[i]);
		i += cnt;
		usleep(5);
	}
	return i;
}

/** Configure the data transmission format
 *
 *  @param mode Clock polarity and phase mode (0 - 3)
 *
 * @code
 * mode | POL PHA
 * -----+--------
 *   0  |  0   0
 *   1  |  0   1
 *   2  |  1   0
 *   3  |  1   1
 * @endcode
 */
void sps_configure(enum sps_mode mode, int clk_mode)
{
	/* Disable All Interrupts */
	GREG32(SPS, ICTRL) = 0;

	GWRITE_FIELD(SPS, CTRL, MODE, mode);
	GWRITE_FIELD(SPS, CTRL, IDLE_LVL, 0);
	GWRITE_FIELD(SPS, CTRL, CPHA, clk_mode & 1);
	GWRITE_FIELD(SPS, CTRL, CPOL, (clk_mode >> 1) & 1);
	GWRITE_FIELD(SPS, CTRL, TXBITOR, 0); /* LSB first */
	GWRITE_FIELD(SPS, CTRL, RXBITOR, 0); /* LSB first */
	/* xfer 0xff when tx fifo is empty */
	GREG32(SPS, DUMMY_WORD) = 0xff;

	/* [5,4,3]           [2,1,0]
	 * RX{DIS, EN, RST} TX{DIS, EN, RST}
	 */
	GREG32(SPS, FIFO_CTRL) = 0x2F;

	GWRITE_FIELD(SPS, ICTRL, TXFIFO_LVL, 0);
	GWRITE_FIELD(SPS, ICTRL, RXFIFO_LVL, 0);
}

int sps_enable(int enable, enum sps_mode mode)
{
	static uint8_t sps_enable_flag;
	if (enable == sps_enable_flag)
		return EC_SUCCESS;

	if (enable) {
		gpio_config_module(MODULE_SPI, 1);
		sps_configure(mode, 0);
	}
	/*
	 * TODO
	else {
	}
	*/
	sps_enable_flag = enable;
	return EC_SUCCESS;
}


static void sps_init(void)
{
	CPRINTS("sps: init clk");
	pmu_clock_en(PERIPH_SPS);
	CPRINTS("sps: enable");
	sps_enable(1, SPS_GENERIC_MODE);
	task_enable_irq(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR);
	task_enable_irq(GC_IRQNUM_SPS0_TXFIFO_LVL_INTR);
	CPRINTS("sps: g_data addr: %p\n", &g_data);
	CPRINTS("sps: init done");
}
DECLARE_HOOK(HOOK_INIT, sps_init, HOOK_PRIO_DEFAULT);



/*****************************************************************************/
/* Interrupt handler stuff */
static void sps_invoke_handler(uint8_t d)
{
	g_data = ~d;
	CPRINTS("sps handler:%02X -> %02X", d, g_data);
}

static void sps_rx_interrupt(int port)
{
	uint8_t d = 0;
	sps_read(port, &d, 1);
	sps_invoke_handler(d);
}
static void sps_tx_interrupt(int port)
{
	uint8_t d = g_data;
	sps_write(port, &d, 1);
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

static int spi_sps_loopback_test(void)
{
#define SELF_TEST_LEN 1
	uint8_t tx_len = SELF_TEST_LEN;
	uint8_t rx_len = SELF_TEST_LEN;
	uint8_t tx_data[SELF_TEST_LEN];
	uint8_t rx_data[SELF_TEST_LEN];

	memset(tx_data, 0xfa, SELF_TEST_LEN);
	memset(rx_data, 0x00, SELF_TEST_LEN);

	CPRINTS("SPS FIFO BASE: %08X\n", SPS_FIFO_BASE_ADDR);

	CPRINTS("SPI TX: %02X", tx_data[0]);
	usleep(10);
	spi_transaction(tx_data, tx_len, rx_data, 0);
	CPRINTS("SPS RX");

	_sps0_rx_interrupt();
	CPRINTS("SPS TX");
	_sps0_tx_interrupt();
	spi_transaction(tx_data, 0, rx_data, rx_len);
	CPRINTS("SPI RX: %02X", rx_data[0]);
	return rx_data[0] == tx_data[0];
}

