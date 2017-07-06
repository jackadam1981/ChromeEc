/* Copyright 2017 The Chromium OS Authors. All rights reserved
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for MEC17xx processor
 */
/** @file gpspi_chip.h
 *MEC17xx General Purpose SPI Master
 */
/** @defgroup MEC17xx gpspi
 */

#ifndef _GPSPI_CHIP_H
#define _GPSPI_CHIP_H

#include <stdint.h>
#include <stddef.h>

/* struct spi_device_t */
#include "spi.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Place any C interfaces here */

int gpspi_transaction_flush(const struct spi_device_t *spi_device);

int gpspi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen);

int gpspi_enable(int port, int enable);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef _GPSPI_CHIP_H */
/**   @}
 */

