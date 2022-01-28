/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ROM_CHIP_H
#define __CROS_EC_ROM_CHIP_H

#include "common.h"

/* Enumerations of ROM api functions */
enum API_RETURN_STATUS_T {
	/* Successful download */
	API_RET_STATUS_OK = 0,
	/* Invalid 3K buffer */
	API_RET_STATUS_BAD_BUF = 0x80000001,
	/* Invalid load interface */
	API_RET_STATUS_BAD_IFC = 0x80000004,
	/* Load interface not ready (eSPI MAF only) */
	API_RET_STATUS_IFC_NOT_READY = 0x80000005,
	/* Bad jump address */
	API_RET_STATUS_BAD_JUMP_ADDR = 0x80000006,
	/* Invalid load size (not a multiple of 64 bytes) */
	API_RET_STATUS_INVAL_SIZE = 0x80000007,
};

/* Load API config 32-bit word */

/* b[7:0] is load source
 * 0 = use load original source ROM used on POR/reset
 * 1 = force load source to be external Shared SPI flash
 * 2 = force load source to be eSPI MAF attached flash
 */
#define ROM_LOAD_API_CFG_IFC_ROM_POS	0
#define ROM_LOAD_API_CFG_IFC_ROM_MSK	0xff
#define ROM_LOAD_API_CFG_IFC_ORIG	0
#define ROM_LOAD_SPI_CFG_IFC_SHD_SPI	1
#define ROM_LOAD_SPI_CFG_IFC_ESPI_MAF	2

/* NOTE: SPI frequency, drive strength, and slew rate are only for SPI
 * interfaces not eSPI.
 */

/* b[9:8] is SPI frequency */
#define ROM_LOAD_API_CFG_SPI_FREQ_POS	8
#define ROM_LOAD_API_CFG_SPI_FREQ_MSK	0x300
#define ROM_LOAD_API_CFG_SPI_FREQ_48M	0
#define ROM_LOAD_API_CFG_SPI_FREQ_24M	0x100
#define ROM_LOAD_API_CFG_SPI_FREQ_16M	0x200
#define ROM_LOAD_API_CFG_SPI_FREQ_12M	0x300

/* b[11:10] is SPI pin drive strength */
#define ROM_LOAD_API_CFG_SPI_STR_POS	10
#define ROM_LOAD_API_CFG_SPI_STR_MSK	0xc00
#define ROM_LOAD_API_CFG_SPI_STR_2MA	0
#define ROM_LOAD_API_CFG_SPI_STR_4MA	0x400
#define ROM_LOAD_API_CFG_SPI_STR_8MA	0x800
#define ROM_LOAD_API_CFG_SPI_STR_12MA	0xc00

/* b[12] is SPI pin slew rate */
#define ROM_LOAD_API_CFG_SPI_SLEW_POS	12
#define ROM_LOAD_API_CFG_SPI_SLEW_FAST	0x1000

/* b[15:13] reserved, must be 0 */
#define ROM_LOAD_SPI_CFG_RSVD1_POS	13
#define ROM_LOAD_SPI_CFG_RSVD1_MSK	0xe000

/* b[19:16] DMA channel used by ROM QMSPI read */
#define ROM_LOAD_SPI_CFG_QDMA_POS	16
#define ROM_LOAD_SPI_CFG_QDMA_MSK	0xf0000
#define ROM_LOAD_SPI_CFG_QDMA_CH0	0
#define ROM_LOAD_SPI_CFG_QDMA_CH1	0x10000
#define ROM_LOAD_SPI_CFG_QDMA_CH2	0x20000
#define ROM_LOAD_SPI_CFG_QDMA_CH3	0x30000
#define ROM_LOAD_SPI_CFG_QDMA_CH4	0x40000
#define ROM_LOAD_SPI_CFG_QDMA_CH5	0x50000
#define ROM_LOAD_SPI_CFG_QDMA_CH6	0x60000
#define ROM_LOAD_SPI_CFG_QDMA_CH7	0x70000
#define ROM_LOAD_SPI_CFG_QDMA_CH8	0x80000
#define ROM_LOAD_SPI_CFG_QDMA_CH9	0x90000
#define ROM_LOAD_SPI_CFG_QDMA_CH10	0xa0000
#define ROM_LOAD_SPI_CFG_QDMA_CH11	0xb0000
#define ROM_LOAD_SPI_CFG_QDMA_CH12	0xc0000
#define ROM_LOAD_SPI_CFG_QDMA_CH13	0xd0000

/* b[29:20] reserved, must be 0 */
#define ROM_LOAD_SPI_CFG_RSVD2_POS	20
#define ROM_LOAD_SPI_CFG_RSVD2_MSK	0x3ff00000

/* b[30] = 0 do not return to caller, 1 = return to caller
 * For do not return, ROM will jump to specified entry point if load
 * was successful.
 */
#define ROM_LOAD_SPI_CFG_RET2CALL_POS	30
#define ROM_LOAD_SPI_CFG_RET2CALL	BIT(30)

/* b[31] = 0 ROM takes over interrupts by reprogramming CM4 vector base
 * to ROM vector table.
 * = 1 Caller owns interrupts (CM4 vector table unchanged)
 */
#define ROM_LOAD_SPI_CFG_CALLER_VT_POS	31
#define ROM_LOAD_SPI_CFG_CALLER_VT	BIT(31)

/* ROM loads from POR interface, 12MHz, ROM does not touch vector table, and
 * ROM returns when done.
 */
#define ROM_LOAD_CFG_12M_JUMP	(ROM_LOAD_API_CFG_IFC_ORIG |		\
				 ROM_LOAD_API_CFG_SPI_FREQ_12M)

/* Load API descriptor */
struct rom_load_descr {
	uint32_t load_address;
	uint32_t length;
	uint32_t spi_address;
	uint32_t entry_point;
};

/* Macro functions of ROM api functions */
#define ADDR_DOWNLOAD_FROM_FLASH (*(volatile uint32_t *) 0xf004)
#define download_from_flash(config, ld_descr_ptr, bufptr) \
	(((download_from_flash_ptr) ADDR_DOWNLOAD_FROM_FLASH) \
	(config, ld_descr_ptr, NULL, NULL, bufptr))

/* Declarations of ROM api functions */
typedef void (*download_from_flash_ptr) (
	uint32_t config,
	struct rom_load_descr *ld_descr,
	uint32_t *ecdsa_pubkey,
	uint32_t *ecdh_prvkey,
	uint32_t *buf3k
);


#endif /* __CROS_EC_ROM_CHIP_H */
