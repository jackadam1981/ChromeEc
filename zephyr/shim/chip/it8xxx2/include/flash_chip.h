/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FLASH_CHIP_H
#define __CROS_EC_FLASH_CHIP_H

/* Flash settings of chip level */
#define CONFIG_INTERNAL_STORAGE
#define CONFIG_MAPPED_STORAGE

#define CONFIG_FLASH_SIZE_BYTES		0x100000
#define CONFIG_MAPPED_STORAGE_BASE	0x80000000
/*
 * One page program instruction allows maximum 256 bytes (a page) of data
 * to be programmed.
 */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE	256
#define CONFIG_FLASH_WRITE_SIZE		0x4     /* minimum write size */
#define CONFIG_FLASH_ERASE_SIZE		0x1000  /* erase bank size */
#define CONFIG_FLASH_BANK_SIZE		0x1000  /* protect bank size */

#define CONFIG_RO_STORAGE_OFF		0x0
#define CONFIG_RW_STORAGE_OFF		0x0

/*
 * The EC uses the one bank of flash to emulate a SPI-like write protect
 * register with persistent state.
 */
#define CONFIG_FLASH_PSTATE
#define CONFIG_FW_PSTATE_SIZE		CONFIG_FLASH_BANK_SIZE
#define CONFIG_FW_PSTATE_OFF		(CONFIG_FLASH_SIZE_BYTES / 2 - \
						CONFIG_FW_PSTATE_SIZE)

#endif /* __CROS_EC_FLASH_CHIP_H */
