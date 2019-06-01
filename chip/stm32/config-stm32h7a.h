/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Memory mapping */
#define CONFIG_FLASH_SIZE       (2 * 1024 * 1024)
#define CONFIG_FLASH_ERASE_SIZE (8 * 1024)  /* erase bank size */
/* always use 128-bit writes due to ECC */
#define CONFIG_FLASH_WRITE_SIZE          16   /* minimum write size */
#define CONFIG_FLASH_WRITE_IDEAL_SIZE    16
#define CONFIG_FLASH_WP_BANKS            4

/*
 * What the code is calling 'bank' is really the size of the block used for
 * write-protected, here it's 8KB sector (same as erase size).
 */
#define CONFIG_FLASH_BANK_SIZE  (8 * 1024)

/* Erasing 128K can take up to 2s, need to defer erase. */
#define CONFIG_FLASH_DEFERRED_ERASE

/*        ITCM-RAM:  64kB 0x00000000 - 0x0000FFFF (CPU and MDMA) */
/*        DTCM-RAM: 128kB 0x20000000 - 0x2001FFFF (CPU and MDMA) */
/* (CD)  AXI-SRAM1: 256kB 0x24000000 - 0x2403FFFF */
/* (CD)  AXI-SRAM2: 384kB 0x2403FFFF - 0x2409FFFF */
/* (CD)  AXI-SRAM3: 384kB 0x2409FFFF - 0x240FFFFF */
/* (CD)  AHB-SRAM1: 64kB  0x30000000 - 0x3000FFFF */
/* (CD)  AHB-SRAM2: 64kB  0x3000FFFF - 0x3001FFFF */
/* (SRD) AHB-SRAM4: 32kB  0x38000000 - 0x38000FFF */
/* (SRD) backup RAM: 4kB  0x38800000 - 0x38800FFF */
#define CONFIG_RAM_BASE		0x24000000
#define CONFIG_RAM_SIZE		((256+384+384)*1024) // AXI-SRAM1-3

#define CONFIG_RO_MEM_OFF	0
#define CONFIG_RO_SIZE		(128 * 1024)
#define CONFIG_RW_MEM_OFF	(CONFIG_FLASH_SIZE / 2)
#define CONFIG_RW_SIZE		(512 * 1024)

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
#define I2C_PORT_COUNT	4

/*
 * Cannot use PSTATE:
 * 128kB blocks are too large and ECC prevents re-writing PSTATE word.
 */
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PSTATE_BANK

/* Number of IRQ vectors on the NVIC */
#define CONFIG_IRQ_COUNT	150

/* the Cortex-M7 core has 'standard' ARMv7-M caches */
#define CONFIG_ARMV7M_CACHE
/* Use the MPU to configure cacheability */
#define CONFIG_MPU
/* Store in uncached buffers for DMA transfers in ahb4 region */
#define CONFIG_CHIP_UNCACHED_REGION ahb
/* Override MPU attribute settings to match the chip requirements */
/* Code is Normal memory type / non-shareable / write-through */
#define MPU_ATTR_FLASH_MEMORY  0x02
/* SRAM Data is Normal memory type / non-shareable / write-back, write-alloc */
#define MPU_ATTR_INTERNAL_SRAM 0x0B
