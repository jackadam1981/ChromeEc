/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Memory mapping STM32G431xb has 128 KBytes of internal flash*/
#define CONFIG_FLASH_SIZE       (256 * 1024)
#define CONFIG_FLASH_WRITE_SIZE 0x0004
#define CONFIG_FLASH_ERASE_SIZE 0x0800
#define CONFIG_FLASH_BANK_SIZE CONFIG_FLASH_SIZE

#define SIZE_2KB (2 * 1024)
#define SIZE_64KB (64 * 1024)
/* Erasing 128K can take up to 2s, need to defer erase. */
#define CONFIG_FLASH_DEFERRED_ERASE

/* No page mode on STM32G4, so no benefit to larger write sizes */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE CONFIG_FLASH_WRITE_SIZE

/* STM32G431x6/x8/xB devices feature 32 Kbytes of embedded SRAM. This SRAM
 * is split into three blocks:
 * • 16 Kbytes mapped at address 0x2000 0000 (SRAM1).
 * • 6 Kbytes mapped at address 0x2000 4000 (SRAM2).
 * • 10 Kbytes mapped at address 0x1000 0000 (CCM SRAM). It is also aliased
 *   at 0x2000 5800 address to be accessed by all masters.
 */
#define CONFIG_RAM_BASE		0x20000000
#define CONFIG_RAM_SIZE		0x00008000

#define CONFIG_RO_MEM_OFF	0
#define CONFIG_RO_SIZE		(CONFIG_FLASH_SIZE/2)
#define CONFIG_RW_MEM_OFF	(CONFIG_RO_MEM_OFF + CONFIG_RO_SIZE)
#define CONFIG_RW_SIZE		(CONFIG_FLASH_SIZE/2)

#define CONFIG_RO_STORAGE_OFF	0
#define CONFIG_RW_STORAGE_OFF	0

#define CONFIG_EC_PROTECTED_STORAGE_OFF		0
#define CONFIG_EC_PROTECTED_STORAGE_SIZE	CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_OFF		CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_SIZE					\
		 (CONFIG_FLASH_SIZE - CONFIG_EC_WRITABLE_STORAGE_OFF)

#define CONFIG_WP_STORAGE_OFF		CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE		CONFIG_EC_PROTECTED_STORAGE_SIZE


#undef I2C_PORT_COUNT
#define I2C_PORT_COUNT	3

/* Number of DMA channels supported (6 channels each for DMA1 and DMA2  */
#define DMAC_COUNT 12

/* Use PSTATE embedded in the RO image, not in its own erase block */
#define CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PSTATE_BANK

/* Use OTP regions */
/* #define CONFIG_OTP */

/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT	101
