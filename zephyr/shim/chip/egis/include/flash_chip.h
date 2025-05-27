/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_CHIP_H
#define __CROS_EC_FLASH_CHIP_H

#define CONFIG_SPI_FLASH_W25Q80 /* Flash connected to SPI. */

#define CONFIG_FLASH_WRITE_SIZE 0x1 /* minimum write size */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE 256 /* one page size for write */

/* RO image offset inside protected storage (RO part) */
#define CONFIG_RO_STORAGE_OFF 0x0

/* RW image offset inside writable storage (RW part) */
#define CONFIG_RW_STORAGE_OFF 0x0

#define CONFIG_FLASH_ERASE_SIZE 4096
/* TODO: what what is usage of it, probably change to 64kb */
#define CONFIG_FLASH_BANK_SIZE 0x10000

#endif /* __CROS_EC_FLASH_CHIP_H */
