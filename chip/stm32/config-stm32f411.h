/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Memory mapping */
#define CONFIG_FLASH_BASE       0x08000000
#define CONFIG_FLASH_PHYSICAL_SIZE (512 * 1024)
#define CONFIG_FLASH_SIZE       CONFIG_FLASH_PHYSICAL_SIZE
#define CONFIG_FLASH_BANK_SIZE  (16 * 1024)
/*
 * 8 "erase" sectors : 16KB/16KB/16KB/16KB/64KB/128KB/128KB/128KB
 * for now, just use 64KB of flash, to maintan fixed sector sizei
 * TODO(gwendal). */
#define CONFIG_FLASH_ERASE_SIZE CONFIG_FLASH_ERASE_SIZE

/* minimum write size for 3.3V. 1 for 1.8V */
#define CONFIG_FLASH_WRITE_SIZE 0x0004

/* No page mode on STM32F, so no benefit to larger write sizes */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE CONFIG_FLASH_WRITE_SIZE

#define CONFIG_RAM_BASE         0x20000000
#define CONFIG_RAM_SIZE         0x00020000

/* Size of one firmware image in flash */
#define CONFIG_FW_IMAGE_SIZE    (128 * 1024)

#define CONFIG_FW_RO_OFF        0
#define CONFIG_FW_RO_SIZE       (CONFIG_FW_IMAGE_SIZE - CONFIG_FW_PSTATE_SIZE)
#define CONFIG_FW_RW_OFF        CONFIG_FW_IMAGE_SIZE
#define CONFIG_FW_RW_SIZE       CONFIG_FW_IMAGE_SIZE
#define CONFIG_FW_WP_RO_OFF     CONFIG_FW_RO_OFF
#define CONFIG_FW_WP_RO_SIZE    CONFIG_FW_IMAGE_SIZE

/*
 * Put PSTATE in OTP block 0.
 */
#define CONFIG_FW_PSTATE_SIZE   32
#define CONFIG_FW_PSTATE_OFF    0x1FFF7800

/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT 85
