/* Copyright 2015 The Chromium OS Authors. All rights reserved.
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
#include "watchdog.h"

struct {
	uint8_t buffer[128];
	unsigned tx_pointer;
	unsigned rx_pointer;
	unsigned command_pointer;
} g_sps_data;

#define SPS_TX_FIFO_BASE_ADDR (GBASE(SPS) + 0x1000)
#define SPS_RX_FIFO_BASE_ADDR (GBASE(SPS) + 0x1400)

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


/* SPI/SPS Statistic Counters */
static uint32_t sps_sts_tx_count;
static uint32_t sps_sts_rx_count;

/* SPI Flash Max transfer size */
#define GC_SPI_FLASH_MAX_XFER_SIZE       (1<<7)

/* SPI Flash Max transfer mask */
#define GC_SPI_FLASH_MAX_XFER_MASK       (GC_SPI_FLASH_MAX_XFER_SIZE - 1)

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

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
 * Start SPI transfer
 */
static void spi_start(uint32_t inst)
{
	GWRITE_FIELD_I(SPI, inst, ISTATE_CLR, TXDONE, 1);
	GWRITE_FIELD_I(SPI, inst, XACT, START, 1);
}

/*
 * Wait for spi tx completion
 */
static int spi_wait_for_tx_done(uint32_t inst)
{
	int cnt = 1000;
	while ((!GREAD_FIELD_I(SPI, inst, ISTATE, TXDONE)) && (--cnt > 0))
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
	spi_start(inst);
	return spi_wait_for_tx_done(inst);
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

/** Configure SPI data transmission format
 *
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

	GWRITE_FIELD(SPI, CTRL, TXBITOR, 0); /* LSB first */
	GWRITE_FIELD(SPI, CTRL, RXBITOR, 0); /* LSB first */

	GREG32(SPI, ICTRL) = 0; /*Tx Interrupt disable */
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

static void spi_init(void)
{
	/* init clock */
	pmu_clock_en(PERIPH_SPI);

	/* Ensure the SPI port is disabled.  This keeps us from interfering
	 * with the main chipset when we're not explicitly using the SPI
	 * bus. */
	spi_enable(1);

	task_enable_irq(GC_IRQNUM_SPI0_SPITXINT);
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

/*
 * Push data to the SPS TX FIFO
 * @param data Pointer to 8-bit data
 * @return push count: 1 or 0
 */
static int sps_push(uint32_t inst, const uint8_t data)
{
	volatile uint8_t *sps_base = (volatile uint8_t *)SPS_TX_FIFO_BASE_ADDR;
	uint32_t offset;
	if (sps_txfifo_full(inst))
		return 0;

	offset = GREG32_I(SPS, inst, TXFIFO_WPTR);
	sps_base[offset] = data;
	GREG32_I(SPS, inst, TXFIFO_WPTR) = offset+1;
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
	volatile uint8_t *sps_base = (volatile uint8_t *)SPS_RX_FIFO_BASE_ADDR;
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
	}
	return i;
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
	GWRITE_FIELD(SPS, CTRL, TXBITOR, 1); /* MSB first */
	GWRITE_FIELD(SPS, CTRL, RXBITOR, 1); /* MSB first */
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

	GREG32(SPS, RXFIFO_THRESHOLD) = 1;
	GREG32(SPS, TXFIFO_THRESHOLD) = 1;
	GWRITE_FIELD(SPS, ICTRL, TXFIFO_LVL, 1);
	GWRITE_FIELD(SPS, ICTRL, RXFIFO_LVL, 1);
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


static void sps_init(void)
{
	pmu_clock_en(PERIPH_SPS);
	sps_enable(1, SPS_GENERIC_MODE);
	task_enable_irq(GC_IRQNUM_SPS0_RXFIFO_LVL_INTR);
	task_enable_irq(GC_IRQNUM_SPS0_TXFIFO_LVL_INTR);
}
DECLARE_HOOK(HOOK_INIT, sps_init, HOOK_PRIO_DEFAULT);



/*****************************************************************************/
/* Interrupt handler stuff */
static void sps_invoke_handler(uint8_t d)
{
	unsigned next_rx = (g_sps_data.rx_pointer + 1) %
		sizeof(g_sps_data.buffer);

	if (next_rx == g_sps_data.tx_pointer)
		return; /* overflow */

	g_sps_data.buffer[g_sps_data.rx_pointer] = d;
	g_sps_data.rx_pointer = next_rx;
}

static void sps_rx_interrupt(int port)
{
	uint8_t d = 0;

	sps_read(port, &d, 1);
	sps_invoke_handler(d);
	sps_sts_rx_count++;
}

static void sps_tx_interrupt(int port)
{
	uint8_t d = 0xff;

	if (g_sps_data.rx_pointer != g_sps_data.tx_pointer) {
		d = g_sps_data.buffer[g_sps_data.tx_pointer];
		g_sps_data.tx_pointer = (g_sps_data.tx_pointer + 1) %
			sizeof(g_sps_data.buffer);
	}

	sps_write(port, &d, 1);
	sps_sts_tx_count++;
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


static int command_spi(int argc, char **argv)
{
	ccprintf("rx count %d, tx count %d\n",
		 sps_sts_rx_count, sps_sts_tx_count);

	if (g_sps_data.command_pointer != g_sps_data.rx_pointer) {
		ccprintf("data received since last time:\n");
		do {
			ccprintf(" %02x", g_sps_data.buffer
				 [g_sps_data.command_pointer++]);
			g_sps_data.command_pointer %= sizeof(g_sps_data.buffer);
		} while(g_sps_data.command_pointer != g_sps_data.rx_pointer);
		ccprintf("\n");
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(spitest, command_spi, "", "", NULL);
