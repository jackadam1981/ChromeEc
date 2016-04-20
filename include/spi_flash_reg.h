/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI flash protection register translation functions for Chrome OS EC.
 */

#ifndef __CROS_EC_SPI_FLASH_REG_H
#define __CROS_EC_SPI_FLASH_REG_H

#include "common.h"

/*
 * Common register bits for SPI flash. All registers / bits may not be valid
 * for all parts.
 */
#define SPI_FLASH_SR2_SUS               (1 << 7)
#define SPI_FLASH_SR2_CMP               (1 << 6)
#define SPI_FLASH_SR2_LB3               (1 << 5)
#define SPI_FLASH_SR2_LB2               (1 << 4)
#define SPI_FLASH_SR2_LB1               (1 << 3)
#define SPI_FLASH_SR2_QE                (1 << 1)
#define SPI_FLASH_SR2_SRP1              (1 << 0)
#define SPI_FLASH_SR1_SRP0              (1 << 7)
#define SPI_FLASH_SR1_SEC               (1 << 6)
#define SPI_FLASH_SR1_TB                (1 << 5)
#define SPI_FLASH_SR1_BP2               (1 << 4)
#define SPI_FLASH_SR1_BP1               (1 << 3)
#define SPI_FLASH_SR1_BP0               (1 << 2)
#define SPI_FLASH_SR1_WEL               (1 << 1)
#define SPI_FLASH_SR1_BUSY              (1 << 0)

/* SR2 register existence based upon chip */
#ifdef CONFIG_SPI_FLASH_W25X40
#undef CONFIG_SPI_FLASH_HAS_SR2
#elif defined(CONFIG_SPI_FLASH_W25Q64) || defined(CONFIG_SPI_FLASH_GD25Q41B)
#define CONFIG_SPI_FLASH_HAS_SR2
#endif

/* Chip type */
enum spi_flash_type {
	SPI_FLASH_W25X40 = 0,
	SPI_FLASH_GD25Q41B,
	SPI_FLASH_W25Q64,
};

#ifndef CONFIG_SPI_FLASH_DEFAULT
#ifdef CONFIG_SPI_FLASH_W25X40
#define CONFIG_SPI_FLASH_DEFAULT SPI_FLASH_W25X40
#elif defined(CONFIG_SPI_FLASH_GD25Q41B)
#define CONFIG_SPI_FLASH_DEFAULT SPI_FLASH_GD25Q41B
#elif defined(CONFIG_SPI_FLASH_W25Q64)
#define CONFIG_SPI_FLASH_DEFAULT SPI_FLASH_W25Q64
#endif
#endif

/**
 * Computes block write protection range from registers
 * Returns start == len == 0 for no protection
 *
 * @param sr1 Status register 1
 * @param sr2 Status register 2
 * @param start Output pointer for protection start offset
 * @param len Output pointer for protection length
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_reg_to_protect(uint8_t sr1, uint8_t sr2, unsigned int *start,
			     unsigned int *len);

/**
 * Computes block write protection registers from range
 *
 * @param start Desired protection start offset
 * @param len Desired protection length
 * @param sr1 Output pointer for status register 1
 * @param sr2 Output pointer for status register 2
 *
 * @return EC_SUCCESS, or non-zero if any error.
 */
int spi_flash_protect_to_reg(unsigned int start, unsigned int len, uint8_t *sr1,
			     uint8_t *sr2);

/* Get SR2 register existence based upon chip */
int spi_flash_has_sr2(void);

#ifdef CONFIG_SPI_FLASH_SELECT
/* Get current selected chip */
int spi_flash_get_selected(void);

/**
 * Select the chip to use for SPI flash. If different devices use the spi_flash
 * interface, they should make sure to negotiate who is in control, and which
 * chip is selected.
 */
void spi_flash_select(enum spi_flash_type spi_select);
#endif
#endif  /* __CROS_EC_SPI_FLASH_REG_H */
