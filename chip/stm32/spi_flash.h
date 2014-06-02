/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI flash interface for Chrome EC */

#ifndef __CROS_EC_SPI_FLASH_H
#define __CROS_EC_SPI_FLASH_H

/**
 * Returns the contents of SPI flash status register 1
 * @return register contents
 */
uint8_t spi_flash_get_status1(void);

/**
 * Returns the contents of SPI flash status register 2
 * @return register contents
 */
uint8_t spi_flash_get_status2(void);

/**
 * Sets the SPI flash status registers (non-volatile bits only)
 * @param reg1	Status register 1
 * @param reg2	Status register 2
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_set_status(int reg1, int reg2);

/**
 * Returns the contents of SPI flash
 * @param buf	Buffer to write flash contents
 * @param offset Flash offset to start reading from
 * @param bytes	Number of bytes to read
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_read(uint8_t *buf, int offset, int bytes);

/**
 * Erase SPI flash.
 * @param offset Flash offset to start erasing
 * @param bytes	Number of bytes to erase
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_erase(int offset, int bytes);

/**
 * Write to SPI flash. Assumes already erased.
 * @param offset Flash offset to write
 * @param bytes Number of bytes to write
 * @param data Data to write to flash
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_write(int offset, int bytes, const uint8_t const *data);

/**
 * Returns the SPI flash manufacturer ID and device ID [8:0]
 * @return flash manufacturer + device ID
 */
uint16_t spi_flash_get_id(void);

/**
 * Returns the SPI flash JEDEC ID (manufacturer ID, memory type, and capacity)
 * @return flash JEDEC ID
 */
uint32_t spi_flash_get_jedec_id(void);

/**
 * Returns the SPI flash unique ID (serial)
 * @return flash unique ID
 */
uint64_t spi_flash_get_unique_id(void);

#endif  /* __CROS_EC_SPI_FLASH_H */
