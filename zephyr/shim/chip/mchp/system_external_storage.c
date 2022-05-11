/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <devicetree.h>
#include <drivers/bbram.h>
#include <soc.h>

#include "clock_chip.h"
#include "common.h"
#include "system.h"
#include "system_chip.h"

#include "config_chip.h"

#define MCHP_ECRO_WORD		0x4F524345u /* ASCII ECRO */
#define MCHP_ECRW_WORD		0x57524345u /* ASCII ECRW */
#define MCHP_PCR_NODE		DT_INST(0, microchip_xec_pcr)

#define GET_BBRAM_OFS(node) \
	DT_PROP(DT_PATH(named_bbram_regions, node), offset)
#define GET_BBRAM_SZ(node) DT_PROP(DT_PATH(named_bbram_regions, node), size)

static const struct device *const bbram_dev =
	COND_CODE_1(DT_HAS_CHOSEN(cros_ec_bbram),
		    DEVICE_DT_GET(DT_CHOSEN(cros_ec_bbram)), NULL);

/* DEBUG values from config_chip.h */
const uint32_t mchp_cfg_ec_tbl[] = {
#ifdef CONFIG_CROS_EC_RO
	MCHP_ECRO_WORD,
#endif
#ifdef CONFIG_CROS_EC_RW
	MCHP_ECRW_WORD,
#endif
	CONFIG_FLASH_SIZE_BYTES,		/* 0x00080000 */
	CONFIG_PROGRAM_MEMORY_BASE,		/* 0x000c0000 */
	CONFIG_RAM_BASE,			/* 0x00118000 */
	CONFIG_RAM_SIZE,			/* 0x00010000 */
	CONFIG_DATA_RAM_SIZE,			/* 0x00010000 from DT_REG_SIZE(DT_CHOSEN(zephyr_sram)) */
	CONFIG_RO_SIZE,
	CONFIG_RW_SIZE,
	CONFIG_RO_MEM_OFF,
	CONFIG_CROS_EC_RO_SIZE,
	CONFIG_RW_MEM_OFF,
	CONFIG_CROS_EC_RW_SIZE,
	CONFIG_WP_STORAGE_OFF,
	CONFIG_WP_STORAGE_SIZE,
	CONFIG_RO_SIZE,
	CONFIG_RW_SIZE,
	CONFIG_EC_PROTECTED_STORAGE_OFF,	/* used by vboot_hash */
	CONFIG_EC_PROTECTED_STORAGE_SIZE,
	CONFIG_EC_WRITABLE_STORAGE_OFF,		/* used by vboot_hash */
	CONFIG_EC_WRITABLE_STORAGE_SIZE,
	CONFIG_RO_STORAGE_OFF,			/* used by vboot_hash */
	CONFIG_RW_STORAGE_OFF,			/* used by vboot_hash */
	CONFIG_PLATFORM_EC_RO_HEADER_OFFSET,
	CONFIG_PLATFORM_EC_RO_HEADER_SIZE,
	CONFIG_RO_MEM_OFF,
	CONFIG_RW_MEM_OFF,
};

/*
 * Make sure CONFIG_XXX flash offsets are correct for MEC172x 512KB SPI flash.
 */
void system_jump_to_booter(void)
{
	static uint32_t flash_offset;
	static uint32_t flash_used;

	__disable_irq();

	for (size_t n = 0; n < ARRAY_SIZE(mchp_cfg_ec_tbl); n++) {
		*(volatile uint32_t *)0x4000FC04u = mchp_cfg_ec_tbl[n];
	}

	/*
	 * Get memory offset and size for RO/RW regions.
	 */
	switch (system_get_shrspi_image_copy()) {
	case EC_IMAGE_RW:
		flash_offset = CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_STORAGE_OFF;
		flash_used = CONFIG_CROS_EC_RW_SIZE;
		break;
	case EC_IMAGE_RO:
	default: /* Jump to RO by default */
#if 0
		flash_offset = CONFIG_PLATFORM_EC_RO_HEADER_SIZE;
		flash_used = (CONFIG_CROS_EC_RO_SIZE - 
			      CONFIG_PLATFORM_EC_RO_HEADER_SIZE);
#else
		flash_offset = CONFIG_PLATFORM_EC_RO_HEADER_OFFSET;
		flash_used = CONFIG_CROS_EC_RO_SIZE; /* this is 0x3F000 */
#endif
		break;
	}

	/*
	 * Speed up FW download time by increasing clock freq of EC. It will
	 * restore to default in clock_init() later.
	 */
	clock_turbo();

	/* MCHP Read selected image from SPI flash into SRAM
	 * Need a jump to little-fw (LFW).
	 * MEC172x Boot-ROM load API is probably not usuable for this.
	 */
	system_download_from_flash(flash_offset,
				   CONFIG_CROS_EC_PROGRAM_MEMORY_BASE,
				   flash_used,
				   (CONFIG_CROS_EC_PROGRAM_MEMORY_BASE + 4u));
}

uint32_t system_get_lfw_address(void)
{
	uint32_t jump_addr = (uint32_t)system_jump_to_booter;
	return jump_addr;
}

enum ec_image system_get_shrspi_image_copy(void)
{
	enum ec_image img = EC_IMAGE_UNKNOWN;
	uint32_t value = 0u;

	if (bbram_dev) {
		if (!bbram_read(bbram_dev, GET_BBRAM_OFS(ec_img_load), 
				GET_BBRAM_SZ(ec_img_load), (uint8_t *)&value)) {
			img = (enum ec_image)(value & 0x7fu);
		}
	}

	if (img == EC_IMAGE_UNKNOWN) {
		img = EC_IMAGE_RO;
		if (mchp_cfg_ec_tbl[0] == MCHP_ECRW_WORD) {
			img = EC_IMAGE_RW;
		}
		system_set_image_copy(img);
	}

	return img;
}

/* Flash is not memory mapped. Store a flag indicating the image.
 * ECS WDT_CNT is register avaible to applications. It implements bits[3:0]
 * which are not reset by a watch dog event only by VTR/chip reset.
 * TODO. VBAT memory is safer only if the board has a stable VBAT power rail. 
 */
void system_set_image_copy(enum ec_image copy)
{
	uint32_t value = (uint32_t)copy;

	if (!bbram_dev) {
		return;
	}

	switch (copy) {
	case EC_IMAGE_RW:
	case EC_IMAGE_RW_B:
		value = EC_IMAGE_RW;
		break;
	case EC_IMAGE_RO:
	default:
		value = EC_IMAGE_RO;
		break;
	}

	bbram_write(bbram_dev, GET_BBRAM_OFS(ec_img_load),
		    GET_BBRAM_SZ(ec_img_load), (uint8_t *)&value);
}
