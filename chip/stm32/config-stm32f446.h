/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Memory mapping */
#define CONFIG_FLASH_BASE		0x08000000
#define CONFIG_FLASH_PHYSICAL_SIZE 	(512 * 1024)
#define CONFIG_FLASH_SIZE		(128 * 1024)
#define CONFIG_FLASH_BANK_SIZE		(16 * 1024)
/*
 * 8 "erase" sectors : 16KB/16KB/16KB/16KB/64KB/128KB/128KB/128KB
 * for now, just use 64KB of flash, to maintan fixed sector size
 * TODO(gwendal). */
#define CONFIG_FLASH_ERASE_SIZE (64 * 1024)

/* minimum write size for 3.3V. 1 for 1.8V */
#define CONFIG_FLASH_WRITE_SIZE_1800 0x0001
#define CONFIG_FLASH_WS_DIV_1800 16000000
#define CONFIG_FLASH_WRITE_SIZE_3300 0x0004
#define CONFIG_FLASH_WS_DIV_3300 30000000
#define CONFIG_FLASH_WRITE_SIZE 0x0004

/* No page mode on STM32F, so no benefit to larger write sizes */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE CONFIG_FLASH_WRITE_SIZE

#define CONFIG_RAM_BASE         0x20000000
/*#define CONFIG_RAM_SIZE         0x00020000*/
#define CONFIG_RAM_SIZE         0x00010000

/* Size of one firmware image in flash */
/*#define CONFIG_IMAGE_SIZE	(64 * 1024)*/

#define CONFIG_RO_MEM_OFF	0
#define CONFIG_RO_SIZE		(48 * 1024)
#define CONFIG_RW_MEM_OFF	(64 * 1024)
#define CONFIG_RW_SIZE		(64 * 1024)
#define CONFIG_WP_RO_OFF	CONFIG_RO_OFF
#define CONFIG_WP_RO_SIZE	CONFIG_IMAGE_SIZE

/*
 * Size of one firmware image in flash - half for RO, half for RW.
 * This is NOT a globally defined config, and is only used in this file
 * for convenience.
 */
#define _IMAGE_SIZE             ((CONFIG_FLASH_SIZE - \
                                  CONFIG_SHAREDLIB_SIZE) / 2)

/*
 * By default, there is no shared objects library.  However, if configured, the
 * shared objects library will be placed after the RO image.
 */
#define CONFIG_SHAREDLIB_MEM_OFF        (CONFIG_RO_MEM_OFF + \
                                         _IMAGE_SIZE)
#define CONFIG_SHAREDLIB_STORAGE_OFF    (CONFIG_RO_STORAGE_OFF + \
                                         _IMAGE_SIZE)
#define CONFIG_SHAREDLIB_SIZE   0

#define CONFIG_RO_STORAGE_OFF   0
#define CONFIG_RW_STORAGE_OFF   0

#define CONFIG_EC_PROTECTED_STORAGE_OFF         0
#define CONFIG_EC_PROTECTED_STORAGE_SIZE        CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_OFF          CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_SIZE         (CONFIG_FLASH_SIZE - \
                                                 CONFIG_EC_WRITABLE_STORAGE_OFF)

#define CONFIG_WP_STORAGE_OFF           CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE          CONFIG_EC_PROTECTED_STORAGE_SIZE


/*
 * Put PSTATE in OTP block 0.
 */
#define CONFIG_FLASH_PSTATE
/*#define CONFIG_FW_PSTATE_SIZE   32
#define CONFIG_FW_PSTATE_OFF    0x1FFF7800*/
#define CONFIG_FW_PSTATE_SIZE	(16 * 1024)
#define CONFIG_FW_PSTATE_OFF	(CONFIG_RO_SIZE)

#undef I2C_PORT_COUNT
#define I2C_PORT_COUNT 4


/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT 97
