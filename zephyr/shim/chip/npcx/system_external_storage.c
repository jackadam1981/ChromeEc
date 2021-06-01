/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/__assert.h>

#include "clock_chip.h"
#include "common.h"
#include "rom_chip.h"
#include "system.h"

/* TODO (b:179900857) Make this implementation not npcx specific. */

#define NPCX_MDC_BASE_ADDR                0x4000C000
#define NPCX_FWCTRL                       REG8(NPCX_MDC_BASE_ADDR + 0x007)
#define NPCX_FWCTRL_RO_REGION             0
#define NPCX_FWCTRL_FW_SLOT               1
#define SET_BIT(reg, bit)           ((reg) |= (0x1 << (bit)))
#define CLEAR_BIT(reg, bit)         ((reg) &= (~(0x1 << (bit))))

/* TODO(b:179900857) Clean this up too */
#undef IS_BIT_SET
#define IS_BIT_SET(reg, bit)        (((reg) >> (bit)) & (0x1))

#if defined(CHIP_FAMILY_NPCX5) || defined(CONFIG_WORKAROUND_FLASH_DOWNLOAD_API)
extern unsigned int __flash_lplfw_start;
extern unsigned int __flash_lplfw_end;
#define LFW_OFFSET		0x160
#define CONFIG_LPRAM_BASE	0x40001400 /* memory address of lpwr ram */
#define CONFIG_LPRAM_SIZE	0x00000620 /* 1568B low power ram */
#define NPCX_PMC_BASE_ADDR	0x4000D000
#define NPCX_GDMA_BASE_ADDR	0x40011000

#define NPCX_PWDWN_CTL_ADDR(offset)    (((offset) < 6) ? \
			(NPCX_PMC_BASE_ADDR + 0x008 + (offset)) : \
			(NPCX_PMC_BASE_ADDR + 0x024))
#define NPCX_PWDWN_CTL(offset)	REG8(NPCX_PWDWN_CTL_ADDR(offset))
#define NPCX_GDMA_TCNT		REG32(NPCX_GDMA_BASE_ADDR + 0x00C)
#define NPCX_LPRAM_CTRL		REG32(0x40001044)
#define NPCX_GDMA_CTL		REG32(NPCX_GDMA_BASE_ADDR + 0x000)
#define NPCX_GDMA_SRCB		REG32(NPCX_GDMA_BASE_ADDR + 0x004)
#define NPCX_GDMA_DSTB		REG32(NPCX_GDMA_BASE_ADDR + 0x008)
#define NPCX_GDMA_CTL_TC	18

enum NPCX_PMC_PWDWN_CTL_T {
	NPCX_PMC_PWDWN_1 = 0,
	NPCX_PMC_PWDWN_2 = 1,
	NPCX_PMC_PWDWN_3 = 2,
	NPCX_PMC_PWDWN_4 = 3,
	NPCX_PMC_PWDWN_5 = 4,
	NPCX_PMC_PWDWN_6 = 5,
	NPCX_PMC_PWDWN_7 = 6,
	NPCX_PMC_PWDWN_CNT,
};

/* Begin address of Suspend RAM for little FW (GDMA utilities). */
uintptr_t __lpram_lfw_start = CONFIG_LPRAM_BASE + LFW_OFFSET;

static void system_download_from_flash(uint32_t srcAddr, uint32_t dstAddr,
		uint32_t size, uint32_t exeAddr)
{
	int i;
	uint8_t chunkSize = 16; /* 4 data burst mode. ie.16 bytes */
	/*
	 * GDMA utility in Suspend RAM. Setting bit 0 of address to indicate
	 * it's a thumb branch for cortex-m series CPU.
	 */
	void (*__start_gdma_in_lpram)(uint32_t) =
			(void(*)(uint32_t))(__lpram_lfw_start | 0x01);

	/*
	 * Before enabling burst mode for better performance of GDMA, it's
	 * important to make sure srcAddr, dstAddr and size of transactions
	 * are 16 bytes aligned in case failure occurs.
	 */
	__ASSERT_NO_MSG((size % chunkSize) == 0 && (srcAddr % chunkSize) == 0 &&
			(dstAddr % chunkSize) == 0);

	/* Check valid address for jumpiing */
	__ASSERT_NO_MSG(exeAddr != 0x0);

	/* Enable power for the Low Power RAM */
	CLEAR_BIT(NPCX_PWDWN_CTL(NPCX_PMC_PWDWN_6), 6);

	/* Enable Low Power RAM */
	NPCX_LPRAM_CTRL = 1;

	/*
	 * Initialize GDMA for flash reading.
	 * [31:21] - Reserved.
	 * [20]    - GDMAERR   = 0  (Indicate GMDA transfer error)
	 * [19]    - Reserved.
	 * [18]    - TC        = 0  (Terminal Count. Indicate operation is end.)
	 * [17]    - Reserved.
	 * [16]    - SOFTREQ   = 0  (Don't trigger here)
	 * [15]    - DM        = 0  (Set normal demand mode)
	 * [14]    - Reserved.
	 * [13:12] - TWS.      = 10 (One double-word for every GDMA transaction)
	 * [11:10] - Reserved.
	 * [9]     - BME       = 1  (4-data ie.16 bytes - Burst mode enable)
	 * [8]     - SIEN      = 0  (Stop interrupt disable)
	 * [7]     - SAFIX     = 0  (Fixed source address)
	 * [6]     - Reserved.
	 * [5]     - SADIR     = 0  (Source address incremented)
	 * [4]     - DADIR     = 0  (Destination address incremented)
	 * [3:2]   - GDMAMS    = 00 (Software mode)
	 * [1]     - Reserved.
	 * [0]     - ENABLE    = 0  (Don't enable yet)
	 */
	NPCX_GDMA_CTL = 0x00002200;

	/* Set source base address */
	NPCX_GDMA_SRCB = CONFIG_MAPPED_STORAGE_BASE + srcAddr;

	/* Set destination base address */
	NPCX_GDMA_DSTB = dstAddr;

	/* Set number of transfers */
	NPCX_GDMA_TCNT = (size / chunkSize);

	/* Clear Transfer Complete event */
	SET_BIT(NPCX_GDMA_CTL, NPCX_GDMA_CTL_TC);

	/* Copy the __start_gdma_in_lpram instructions to LPRAM */
	for (i = 0; i < &__flash_lplfw_end - &__flash_lplfw_start; i++)
		*((uint32_t *)__lpram_lfw_start + i) =
				*(&__flash_lplfw_start + i);

	/* Start GDMA in Suspend RAM */
	__start_gdma_in_lpram(exeAddr);
}
#endif /* CHIP_FAMILY_NPCX5 || CONFIG_WORKAROUND_FLASH_DOWNLOAD_API */

void system_jump_to_booter(void)
{
	enum API_RETURN_STATUS_T status __attribute__((unused));
	static uint32_t flash_offset;
	static uint32_t flash_used;
	static uint32_t addr_entry;

	/*
	 * Get memory offset and size for RO/RW regions.
	 * Both of them need 16-bytes alignment since GDMA burst mode.
	 */
	switch (system_get_shrspi_image_copy()) {
	case EC_IMAGE_RW:
		flash_offset = CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_STORAGE_OFF;
		flash_used = CONFIG_RW_SIZE;
		break;
#ifdef CONFIG_RW_B
	case EC_IMAGE_RW_B:
		flash_offset = CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_B_STORAGE_OFF;
		flash_used = CONFIG_RW_SIZE;
		break;
#endif
	case EC_IMAGE_RO:
	default: /* Jump to RO by default */
		flash_offset = CONFIG_EC_PROTECTED_STORAGE_OFF +
				CONFIG_RO_STORAGE_OFF;
		flash_used = CONFIG_RO_SIZE;
		break;
	}

	/* Make sure the reset vector is inside the destination image */
	addr_entry = *(uintptr_t *)(flash_offset +
				    CONFIG_MAPPED_STORAGE_BASE + 4);

	/*
	 * Speed up FW download time by increasing clock freq of EC. It will
	 * restore to default in clock_init() later.
	 */
	clock_turbo();

/*
 * npcx9 Rev.1 has the problem for download_from_flash API.
 * Workwaroud it by executing the system_download_from_flash function
 * in the suspend RAM like npcx5.
 * TODO: Removing npcx9 when Rev.2 is available.
 */
	/* Bypass for GMDA issue of ROM api utilities */
#if defined(CHIP_FAMILY_NPCX5) || defined(CONFIG_WORKAROUND_FLASH_DOWNLOAD_API)
	system_download_from_flash(
		flash_offset,      /* The offset of the data in spi flash */
		CONFIG_PROGRAM_MEMORY_BASE, /* RAM Addr of downloaded data */
		flash_used,        /* Number of bytes to download      */
		addr_entry         /* jump to this address after download */
	);
#else
	download_from_flash(
		flash_offset,      /* The offset of the data in spi flash */
		CONFIG_PROGRAM_MEMORY_BASE, /* RAM Addr of downloaded data */
		flash_used,        /* Number of bytes to download      */
		SIGN_NO_CHECK,     /* Need CRC check or not               */
		addr_entry,        /* jump to this address after download */
		&status            /* Status fo download */
	);
#endif
}

uint32_t system_get_lfw_address()
{
	/*
	 * In A3 version, we don't use little FW anymore
	 * We provide the alternative function in ROM
	 */
	uint32_t jump_addr = (uint32_t)system_jump_to_booter;
	return jump_addr;
}

enum ec_image system_get_shrspi_image_copy(void)
{
	/* TODO (b:179900857) Make this implementation not npcx specific. */
	if (IS_BIT_SET(NPCX_FWCTRL, NPCX_FWCTRL_RO_REGION)) {
		/* RO image */
#ifdef CHIP_HAS_RO_B
		if (!IS_BIT_SET(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT))
			return EC_IMAGE_RO_B;
#endif
		return EC_IMAGE_RO;
	} else {
#ifdef CONFIG_RW_B
		/* RW image */
		if (!IS_BIT_SET(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT))
			/* Slot A */
			return EC_IMAGE_RW_B;
#endif
		return EC_IMAGE_RW;
	}
}

void system_set_image_copy(enum ec_image copy)
{
	/* TODO (b:179900857) Make this implementation not npcx specific. */
	switch (copy) {
	case EC_IMAGE_RW:
		CLEAR_BIT(NPCX_FWCTRL, NPCX_FWCTRL_RO_REGION);
		SET_BIT(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT);
		break;
#ifdef CONFIG_RW_B
	case EC_IMAGE_RW_B:
		CLEAR_BIT(NPCX_FWCTRL, NPCX_FWCTRL_RO_REGION);
		CLEAR_BIT(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT);
		break;
#endif
	default:
		/* Fall through to EC_IMAGE_RO */
	case EC_IMAGE_RO:
		SET_BIT(NPCX_FWCTRL, NPCX_FWCTRL_RO_REGION);
		SET_BIT(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT);
		break;
	}
}
