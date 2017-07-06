/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI master module for MEC1322 */

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
#include "qmspi_chip.h"

#ifdef CONFIG_MEC1701_GP_SPI
#include "gpspi_chip.h"
#endif

#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define SPI_BYTE_TRANSFER_TIMEOUT_US (3 * MSEC)
#define SPI_BYTE_TRANSFER_POLL_INTERVAL_US 100


/* only regular image needs mutex, LFW does not have scheduling */
#ifndef LFW
static struct mutex spi_mutex;
#endif


/*
 * Public SPI interface
 */

int spi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

	switch (spi_device->port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_transaction_async(spi_device, txdata,
				txlen, rxdata, rxlen);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_transaction_async(spi_device, txdata,
				txlen, rxdata, rxlen);
		break;
	default:
		rc = EC_ERROR_INVAL;
	}

	return rc;
}

int spi_transaction_flush(const struct spi_device_t *spi_device)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

	switch (spi_device->port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_transaction_flush(spi_device);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_transaction_flush(spi_device);
		break;
	default:
		rc = EC_ERROR_INVAL;
	}

	return rc;
}


/*
 * called from common/spi_flash.c
 */
int spi_transaction(const struct spi_device_t *spi_device,
		    const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

#ifndef LFW
	mutex_lock(&spi_mutex);
#endif

	rc = spi_transaction_async(spi_device, txdata, txlen, rxdata, rxlen);

	if ((rc == EC_SUCCESS) && (rxlen > 0))
		rc = spi_transaction_flush(spi_device);

#ifndef LFW
	mutex_unlock(&spi_mutex);
#endif
	return rc;
}


/*
 * called from common/spi_flash.c
 */
int spi_enable(int port, int enable)
{
	int rc;

	switch (port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_enable(port, enable);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_enable(port, enable);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return rc;
}

