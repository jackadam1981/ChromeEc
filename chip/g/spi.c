/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "shared_mem.h"
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
		/* CSB to SCK setup time in SCK cycles + 1.5 */
		GWRITE_FIELD(SPI, CTRL, CSBSU, 1);
		/* CSB from SCK hold time in SCK cycles + 1 */
		GWRITE_FIELD(SPI, CTRL, CSBHLD, 1);
		/* SPI clk divider */
		GWRITE_FIELD(SPI, CTRL, IDIV,    7);
		GWRITE_FIELD(SPI, CTRL, TXBITOR, 0); /* LSB first */
		GWRITE_FIELD(SPI, CTRL, RXBITOR, 0); /* LSB first */
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
	/* init clock */
	pmu_clock_en(PERIPH_SPI);

	/* Ensure the SPI port is disabled.  This keeps us from interfering
	 * with the main chipset when we're not explicitly using the SPI
	 * bus. */
	spi_enable(0);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);

