/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_CHIP_H
#define __CROS_EC_FLASH_CHIP_H

#define CONFIG_SPI_FLASH_W25Q80 /* Internal SPI flash type. */

/*
 * One page program instruction allows maximum 256 bytes (a page) of data
 * to be programmed.
 */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE 256
/* Minimum write size */
#define CONFIG_FLASH_WRITE_SIZE \
	DT_PROP(DT_INST(0, soc_nv_flash), write_block_size)
/* Erase bank size */
#define CONFIG_FLASH_ERASE_SIZE \
	DT_PROP(DT_INST(0, soc_nv_flash), erase_block_size)
/* Protect bank size */
#define CONFIG_FLASH_BANK_SIZE CONFIG_FLASH_ERASE_SIZE

#define CONFIG_RO_STORAGE_OFF 0x20
#define CONFIG_RW_STORAGE_OFF 0x0

#endif /* __CROS_EC_FLASH_CHIP_H */
