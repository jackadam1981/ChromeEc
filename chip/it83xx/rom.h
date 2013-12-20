/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Header for ROM code. */

#ifndef _CROS_EC_ROM_H
#define _CROS_EC_ROM_H

#define FLASH_INTERNAL 0x4f
#define FLASH_EXTERNAL 0x0f

/* Various helpful SPI flash routines are in ROM at this base address. */
#define ROM_BASE 0x70000

/**
 * Read status (command 0x05)
 *
 * @param Flash selection
 *
 * @return Flash status
 */
unsigned char (*spi_read_status)(unsigned char) =
		(unsigned char (*)(unsigned char))(ROM_BASE + 0x13a);

/**
 * Write status (command 0x01)
 *
 * @param Flash selection
 * @param Status to be set
 * @param Enable Write Status Reg: for SST flash chip(ID 0xBF), must be set
 */
void (*spi_write_status)(unsigned char, unsigned char, unsigned char) =
		(void (*)(unsigned char, unsigned char, unsigned char))
		(ROM_BASE + 0x17a);

/**
 * Read ID (command 0x9f)
 *
 * @param Flash selection
 * @param Pointer to ID buffer
 */
void (*spi_read_id)(unsigned char, unsigned char*) =
		(void (*)(unsigned char, unsigned char*))(ROM_BASE + 0x21a);

/**
 * Read ID (command 0xab)
 *
 * @param Flash selection
 * @param Pointer to ID buffer
 */
void (*spi_read_id_cmd_ab)(unsigned char, unsigned char*) =
		(void (*)(unsigned char, unsigned char*))(ROM_BASE + 0x2a4);

/**
 * Erase flash
 *
 * @param Flash selection
 * @param Erase command:
 *		0xd7: erase 1k bytes
 *		0x20: erase 4k bytes
 *		0x52: erase 32k bytes
 *		0xd8: erase 64k bytes
 * @param Flash address
 */
void (*spi_erase)(unsigned char, unsigned char, unsigned long) =
		(void (*)(unsigned char, unsigned char, unsigned long))
		(ROM_BASE + 0x336);

/**
 * Write Enable
 *
 * @param Flash selection
 * @param Enable Write Status Reg: for SST flash chip(ID 0xBF), must be set
 */
void (*spi_write_enable)(unsigned char, unsigned char) =
		(void (*)(unsigned char, unsigned char))(ROM_BASE + 0x3cc);

/**
 * Write Disable
 *
 * @param Flash selection
 */
void (*spi_write_disable)(unsigned char) =
		(void (*)(unsigned char))(ROM_BASE + 0x464);

/**
 * Write Byte
 *
 * @param Flash selection
 * @param Flash address
 * @param Pointer to data buffer
 * @param Number of bytes to write
 */
void (*spi_write_byte)(unsigned char, unsigned long, const unsigned char*,
		unsigned long) =
		(void (*)(unsigned char, unsigned long, const unsigned char*,
				unsigned long))(ROM_BASE + 0x4dc);

/**
 * Write data to flash (AAI word program)
 *
 * @param Flash selection
 * @param Flash address
 * @param Pointer to data buffer
 * @param Number of bytes to write
 */
void (*spi_write_aai_word)(unsigned char, unsigned long, const unsigned char*,
		unsigned long) =
		(void (*)(unsigned char, unsigned long, const unsigned char*,
				unsigned long))(ROM_BASE + 0x59a);

/**
 * Write data to flash (AAI program)
 *
 * @param Flash selection
 * @param Flash address
 * @param Pointer to data buffer
 * @param Number of bytes to write
 */
void (*spi_write_aai)(unsigned char, unsigned long, const unsigned char*,
		unsigned long) =
		(void (*)(unsigned char, unsigned long, const unsigned char*,
				unsigned long))(ROM_BASE + 0x69e);

/**
 * EC-indirect fast read
 *
 * @param Flash selection
 * @param Flash address
 * @param Pointer to data buffer
 * @param Number of bytes to read
 */
void (*spi_ec_indirect_fast_read)(unsigned char, unsigned long,
		unsigned char*, unsigned long) =
		(void (*)(unsigned char, unsigned long, unsigned char*,
				unsigned long))(ROM_BASE + 0x78a);

/**
 * Rescan the signature and write protect settings in eFlash
 */
void (*eflash_rescan_signature)(void) =
		(void (*)(void))(ROM_BASE + 0x8ee);

#endif /* _CROS_EC_ROM_H */



