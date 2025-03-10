/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_CHIP_H
#define __CROS_EC_FLASH_CHIP_H

// TODO double verify that
/* Minimum write size */
#define CONFIG_FLASH_WRITE_SIZE 4

/* No page mode, so use minimum write size */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE CONFIG_FLASH_WRITE_SIZE

/* RO image offset inside protected storage (RO part) */
#define CONFIG_RO_STORAGE_OFF 0x0

/* RW image offset inside writable storage (RW part) */
#define CONFIG_RW_STORAGE_OFF 0x0

// TODO double verify that, get it from DTS?
// It is per project vaule/flash chip, not per chip family
#define CONFIG_FLASH_ERASE_SIZE 4096
#define CONFIG_FLASH_BANK_SIZE 4096

#endif /* __CROS_EC_FLASH_CHIP_H */
