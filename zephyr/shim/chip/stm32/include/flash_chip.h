/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_CHIP_H
#define __CROS_EC_FLASH_CHIP_H

/* Minimum write size supported by zephyr is 1 byte*/
#define CONFIG_FLASH_WRITE_SIZE 0x1
/* No page mode on STM32F, so no benefit to larger write sizes */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE CONFIG_FLASH_WRITE_SIZE

/* Undef erase size and bank size because STM32F4 has different sector sizes */
#undef CONFIG_FLASH_ERASE_SIZE
#undef CONFIG_FLASH_BANK_SIZE

#define CONFIG_RO_STORAGE_OFF 0x0
#define CONFIG_RW_STORAGE_OFF 0x0

#endif /* __CROS_EC_FLASH_CHIP_H */
