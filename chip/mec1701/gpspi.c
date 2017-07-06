/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* General Purpose SPI master module for MEC1701 */

#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "timer.h"
#include "util.h"
#include "hooks.h"
#include "task.h"
#include "gpspi_chip.h"

#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define SPI_BYTE_TRANSFER_TIMEOUT_US (3 * MSEC)
#define SPI_BYTE_TRANSFER_POLL_INTERVAL_US 100

/*
 * GP-SPI
 */
static const struct dma_option gpspi_rx_option[] = {
	{
		MEC17XX_DMAC_SPI0_RX,
		(void *)&MEC17XX_SPI_RD(0),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
	{
		MEC17XX_DMAC_SPI1_RX,
		(void *)&MEC17XX_SPI_RD(1),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
};

static int gpspi_wait_byte(const int port)
{
	timestamp_t deadline;

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	while ((MEC17XX_SPI_SR(port) & 0x3) != 0x3) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}

static int gpspi_tx(const int port, const uint8_t *txdata, int txlen)
{
	int i;
	int ret;
	uint8_t dummy = 0;

	ret = EC_SUCCESS;
	for (i = 0; i < txlen; ++i) {
		MEC17XX_SPI_TD(port) = txdata[i];
		ret = gpspi_wait_byte(port);
		if (ret != EC_SUCCESS)
			return ret;
		dummy += MEC17XX_SPI_RD(port);
	}

	return ret;
}

int gpspi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	int port = spi_device->port;
	int ret = EC_SUCCESS;

	gpio_set_level(spi_device->gpio_cs, 0);

	/* Disable auto read */
	MEC17XX_SPI_CR(port) &= ~(1 << 5);

	ret = gpspi_tx(port, txdata, txlen);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable auto read */
	MEC17XX_SPI_CR(port) |= 1 << 5;

	if (rxlen != 0) {
		dma_start_rx(&gpspi_rx_option[port], rxlen, rxdata);
		MEC17XX_SPI_TD(port) = 0;
	}
	return ret;
}

int gpspi_transaction_flush(const struct spi_device_t *spi_device)
{
	int port = spi_device->port;
	enum dma_channel channel = gpspi_rx_option[port].channel;
	int ret = dma_wait(channel);

	timestamp_t deadline;

	/* Disable auto read */
	MEC17XX_SPI_CR(port) &= ~(1 << 5);

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	/* Wait for FIFO empty SPISR_TXBE */
	while ((MEC17XX_SPI_SR(port) & 0x01) != 0x1) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}

	dma_disable(channel);
	dma_clear_isr(channel);
	if (MEC17XX_SPI_SR(port) & 0x2)
		port = MEC17XX_SPI_RD(port);

	gpio_set_level(spi_device->gpio_cs, 1);

	return ret;
}

int gpspi_enable(int port, int enable)
{
	if (enable) {
		gpio_config_module(MODULE_SPI_MASTER, 1);

		/* Set enable bit in SPI_AR */
		MEC17XX_SPI_AR(port) |= 0x1;

		/* Set SPDIN to 0 -> Full duplex */
		MEC17XX_SPI_CR(port) &= ~(0x3 << 2);

		/* Set CLKPOL, TCLKPH, RCLKPH to 0 */
		MEC17XX_SPI_CC(port) &= ~0x7;

		/* Set LSBF to 0 -> MSB first */
		MEC17XX_SPI_CR(port) &= ~0x1;
	} else {
		/* Clear enable bit in SPI_AR */
		MEC17XX_SPI_AR(port) &= ~0x1;

		gpio_config_module(MODULE_SPI_MASTER, 0);
	}

	return EC_SUCCESS;
}

