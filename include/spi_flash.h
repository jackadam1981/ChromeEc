/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI flash interface for Chrome EC */

#ifndef __CROS_EC_SPI_FLASH_H
#define __CROS_EC_SPI_FLASH_H

/**
 * Probe for supported SPI flash chip
 *
 * @return EC_SUCCESS if supported SPI flash chip is found. Non-zero if not.
 */
int spi_flash_probe(void);

/**
 * Read data from SPI flash
 *
 * @param src_addr 24-bit flash address to read from.
 * @param dest Pointer to destination address.
 * @param size Number of bytes to read.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int spi_flash_read(uint32_t src_addr, uint8_t *dest, int size);

#endif  /* __CROS_EC_SPI_FLASH_H */
