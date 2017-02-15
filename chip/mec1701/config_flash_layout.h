/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_FLASH_LAYOUT_H
#define __CROS_EC_CONFIG_FLASH_LAYOUT_H

/*
 * mec17xx flash layout:
 * - Non memory-mapped, external SPI.
 * - RW image at the beginning of writable region.
 * - Bootloader at the beginning of protected region, followed by RO image.
 * - Loader + (RO | RW) loaded into program memory.
 */

/* Non-memmapped, external SPI */
#define CONFIG_EXTERNAL_STORAGE
#undef  CONFIG_MAPPED_STORAGE
#undef  CONFIG_FLASH_PSTATE
#define CONFIG_SPI_FLASH

/* TODO - 0x20000 = 128KB. Change to 192KB = 0x30000 */
/* EC region of SPI resides at end of ROM, protected region follows writable */
#if 0
#define CONFIG_EC_PROTECTED_STORAGE_OFF  (CONFIG_FLASH_SIZE - 0x20000)
#define CONFIG_EC_PROTECTED_STORAGE_SIZE 0x20000
#define CONFIG_EC_WRITABLE_STORAGE_OFF   (CONFIG_FLASH_SIZE - 0x40000)
#define CONFIG_EC_WRITABLE_STORAGE_SIZE  0x20000
#else
/* Locate EC_RO at TopOfFlash - 192KB */
/* #define CONFIG_EC_PROTECTED_STORAGE_OFF  (CONFIG_FLASH_SIZE - 0x30000) */
/* MEC1701 lfw + EC_RO at start of second 4KB sector */
#define CONFIG_EC_PROTECTED_STORAGE_OFF	0x1000
/* EC_RO flash region length = 192KB */
#define CONFIG_EC_PROTECTED_STORAGE_SIZE 0x30000
/* Locate EC_RW at TopOfFlash - 384KB */
/* #define CONFIG_EC_WRITABLE_STORAGE_OFF   (CONFIG_FLASH_SIZE - 0x60000) */
/* Locate EC_RW at 256KB from start of flash */
#define CONFIG_EC_WRITABLE_STORAGE_OFF 0x40000
/* EC_RW flash region length = 192KB */
#define CONFIG_EC_WRITABLE_STORAGE_SIZE  0x30000
#endif

/* Loader resides at the beginning of program memory */
#define CONFIG_LOADER_MEM_OFF		0
/* #define CONFIG_LOADER_SIZE		0xC00 */
#define CONFIG_LOADER_SIZE		0x1000

/* Write protect Loader and RO Image */
#define CONFIG_WP_STORAGE_OFF		CONFIG_EC_PROTECTED_STORAGE_OFF
/*
 * TODO MCHP - New size.
 *  Write protect 128k section of 256k physical flash which contains loader
 * and RO Images.
 * TODO - New memory size of 192KB (128 + 64)
 */
#define CONFIG_WP_STORAGE_SIZE		CONFIG_EC_PROTECTED_STORAGE_SIZE

/*
 * RO / RW images follow the loader in program memory. Either RO or RW
 * image will be loaded -- both cannot be loaded at the same time.
 */
#define CONFIG_RO_MEM_OFF		(CONFIG_LOADER_MEM_OFF + \
					CONFIG_LOADER_SIZE)
/* MEC1701 Code + Data SRAM size = 256KB
 * Allocate 32KB for data leaving 192KB for Code.
 * The loader is resident in first 3KB of Code SRAM.
 * Loader can load 192 - 4 = 188 KB maximum.
 * !!! This size MUST be a multiple of flash erase block size.
 * defined by CONFIG_FLASH_ERASE_SIZE in chip/config_chip.4 !!!
 */
#define CONFIG_RO_SIZE			(188 * 1024)
#define CONFIG_RW_MEM_OFF		CONFIG_RO_MEM_OFF
#define CONFIG_RW_SIZE			CONFIG_RO_SIZE

/* WP region consists of second half of SPI, and begins with the boot header
 * MEC1701 Boot-ROM differs from MEC1322.
 * Two 4-byte TAG's exist at offset 0 and 4 in the SPI flash device.
 * Each tag points to Header of 128 bytes.
 * The Header must be aligned on a 256-byte or greater boundary in the
 * SPI flash device.
 * We will locate the Header at the beginning of the second 4KB block since
 * many SPI flash devices' miminum erase block size is 4KB.
 */
#define CONFIG_BOOT_HEADER_STORAGE_OFF	0
#define CONFIG_BOOT_HEADER_STORAGE_SIZE	0x80

/* Loader / lfw image immediately follows the boot header on SPI */
#define CONFIG_LOADER_STORAGE_OFF	(CONFIG_BOOT_HEADER_STORAGE_OFF + \
					CONFIG_BOOT_HEADER_STORAGE_SIZE)

/* RO image immediately follows the loader image */
#define CONFIG_RO_STORAGE_OFF		(CONFIG_LOADER_STORAGE_OFF + \
					CONFIG_LOADER_SIZE)

/* RW image starts at the beginning of SPI */
/* #define CONFIG_RW_STORAGE_OFF		0 */
/* RW region has 128 byte header pre-pended to RW binary */
#define CONFIG_RW_STORAGE_OFF		(CONFIG_BOOT_HEADER_STORAGE_OFF + \
					CONFIG_BOOT_HEADER_STORAGE_SIZE)


#endif /* __CROS_EC_CONFIG_FLASH_LAYOUT_H */
