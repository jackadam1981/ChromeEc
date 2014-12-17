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

int sps_enable(int enable)
{
	static uint8_t sps_enable_flag;
	if (enable == sps_enable_flag)
		return EC_SUCCESS;

	if (enable) {
		gpio_config_module(MODULE_SPI, 1);

		/* Configure SPI Slave: Swetland mode */
		GWRITE_FIELD(SPS, CTRL, MODE, 1);
		GWRITE_FIELD(SPS, CTRL, CPOL, 0);
		GWRITE_FIELD(SPS, CTRL, CPHA, 0);
		GWRITE_FIELD(SPS, CTRL, IDLE_LVL, 0);
		GWRITE_FIELD(SPS, CTRL, TXBITOR, 0); /* LSB first */
		GWRITE_FIELD(SPS, CTRL, RXBITOR, 0); /* LSB first */
		GWRITE_FIELD(SPS, CTRL, ROM_ADDR_SIZE, 3);

		/* xfer 0xff when tx fifo is empty */
		GREG32(SPS, DUMMY_WORD) = 0xff;

		/* ROM Region { 0, 1, 2, 3 }: delta offset: 0x14 */
		GWRITE_FIELD(SPS, ROM_REGION0_CTRL, READ_EN, 1);
		GWRITE_FIELD(SPS, ROM_REGION0_CTRL, WRITE_EN, 0);
		GREG32(SPS, ROM_REGION0_ROM_BASE) = 0;
		GREG32(SPS, ROM_REGION0_SIZE) = 128;

		/* ROM Mem CMD {OP, ADDR, REGION, LEN} TBD */

		/* Disable All Interrupts */
		GREG32(SPS, ICTRL) = 0;
	}
	/*
	 * TODO
	else {
	}
	*/
	sps_enable_flag = enable;
	return EC_SUCCESS;
}


int spi_transaction(const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int rc = EC_SUCCESS;
	uint32_t inst = 0;

	if (txdata)
		rc = spi_write(inst, txdata, txlen);
	if (rc)
		return rc;

	if (rxdata)
		spi_read(inst, rxdata, txlen);

	return EC_SUCCESS;
}

/* Hooks */
static void spi_init(void)
{
#if 0
	/* init clock */
	CPRINTS("spi: init clk");
	pmu_clock_en(PERIPH_SPI);

	/* Ensure the SPI port is disabled.  This keeps us from interfering
	 * with the main chipset when we're not explicitly using the SPI
	 * bus. */
	spi_enable(0);
#endif

}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);

static void sps_init(void)
{
#if 0
	CPRINTS("sps: init clk");
	pmu_clock_en(PERIPH_SPS);
	CPRINTS("sps: enable");
	sps_enable(1);
	CPRINTS("sps: init done");
#endif
}
DECLARE_HOOK(HOOK_INIT, sps_init, HOOK_PRIO_DEFAULT);
