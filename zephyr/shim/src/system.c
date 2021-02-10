/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/cros_bbram.h>

#include "bbram.h"
#include "common.h"
#include "cros_version.h"
#include "system.h"

#define BBRAM_REGION_PD0	DT_PATH(named_bbram_regions, pd0)
#define BBRAM_REGION_PD1	DT_PATH(named_bbram_regions, pd1)
#define BBRAM_REGION_PD2	DT_PATH(named_bbram_regions, pd2)
#define BBRAM_REGION_TRY_SLOT	DT_PATH(named_bbram_regions, try_slot)

STATIC_IF_NOT(CONFIG_ZTEST) const struct device *bbram_dev;

#if DT_NODE_EXISTS(DT_NODELABEL(bbram))
static int system_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bbram_dev = device_get_binding(DT_LABEL(DT_NODELABEL(bbram)));
	return 0;
}

SYS_INIT(system_init, PRE_KERNEL_1, 50);
#endif

/* Map idx to a bbram offset/size, or return -1 on invalid idx */
static int bbram_lookup(enum system_bbram_idx idx, int *offset_out,
			int *size_out)
{
	switch (idx) {
	case SYSTEM_BBRAM_IDX_PD0:
		*offset_out = DT_PROP(BBRAM_REGION_PD0, offset);
		*size_out = DT_PROP(BBRAM_REGION_PD0, size);
		break;
	case SYSTEM_BBRAM_IDX_PD1:
		*offset_out = DT_PROP(BBRAM_REGION_PD1, offset);
		*size_out = DT_PROP(BBRAM_REGION_PD1, size);
		break;
	case SYSTEM_BBRAM_IDX_PD2:
		*offset_out = DT_PROP(BBRAM_REGION_PD2, offset);
		*size_out = DT_PROP(BBRAM_REGION_PD2, size);
		break;
	case SYSTEM_BBRAM_IDX_TRY_SLOT:
		*offset_out = DT_PROP(BBRAM_REGION_TRY_SLOT, offset);
		*size_out = DT_PROP(BBRAM_REGION_TRY_SLOT, size);
		break;
	default:
		return EC_ERROR_INVAL;
	}
	return EC_SUCCESS;
}

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	int offset, size, rc;

	if (bbram_dev == NULL)
		return EC_ERROR_INVAL;

	rc = bbram_lookup(idx, &offset, &size);
	if (rc)
		return rc;

	rc = ((struct cros_bbram_driver_api *)bbram_dev->api)
		     ->read(bbram_dev, offset, size, value);
	return rc ? EC_ERROR_INVAL : EC_SUCCESS;
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
	/*
	 * TODO(b:173787365): implement this.  For now, doing nothing
	 * won't break anything, just will eat power.
	 */
}

const char *system_get_chip_vendor(void)
{
	return "chromeos";
}

const char *system_get_chip_name(void)
{
	return "emu";
}

const char *system_get_chip_revision(void)
{
	return "";
}
#ifdef CONFIG_PLATFORM_EC_EXTERNAL_STORAGE

/*
 * TODO (b:179900857) Make this implementation not npcx specific.
 */
#define NPCX_MDC_BASE_ADDR                0x4000C000
#define NPCX_FWCTRL                       REG8(NPCX_MDC_BASE_ADDR + 0x007)
#define NPCX_FWCTRL_RO_REGION             0
#define NPCX_FWCTRL_FW_SLOT               1
#define SET_BIT(reg, bit)           ((reg) |= (0x1 << (bit)))
#define CLEAR_BIT(reg, bit)         ((reg) &= (~(0x1 << (bit))))
#define IS_BIT_SET(reg, bit)        (((reg) >> (bit)) & (0x1))
enum API_RETURN_STATUS_T {
	/* Successful download */
	API_RET_STATUS_OK = 0,
	/* Address is outside of flash or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_SRC_ADDR = 1,
	/* Address is outside of RAM or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_DST_ADDR = 2,
	/* Size is 0 or not 4 bytes aligned. */
	API_RET_STATUS_INVALID_SIZE = 3,
	/* Flash Address + Size is out of flash. */
	API_RET_STATUS_INVALID_SIZE_OUT_OF_FLASH = 4,
	/* RAM Address + Size is out of RAM. */
	API_RET_STATUS_INVALID_SIZE_OUT_OF_RAM = 5,
	/* Wrong sign option. */
	API_RET_STATUS_INVALID_SIGN = 6,
	/* Error during Code copy. */
	API_RET_STATUS_COPY_FAILED = 7,
	/* Execution Address is outside of RAM */
	API_RET_STATUS_INVALID_EXE_ADDR = 8,
	/* Bad CRC value */
	API_RET_STATUS_INVALID_SIGNATURE = 9,
};

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
	/*
	 * TODO (b:179900857) Make this implementation not npcx specific.
	 */
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
	/*
	 * TODO (b:179900857) Make this implementation not npcx specific.
	 */
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
		CPRINTS("Invalid copy (%d) is requested as a jump destination. "
			"Change it to %d.", copy, EC_IMAGE_RO);
		/* Fall through to EC_IMAGE_RO */
	case EC_IMAGE_RO:
		SET_BIT(NPCX_FWCTRL, NPCX_FWCTRL_RO_REGION);
		SET_BIT(NPCX_FWCTRL, NPCX_FWCTRL_FW_SLOT);
		break;
	}
}
#endif /* CONFIG_PLATFORM_EC_EXTERNAL_STORAGE */

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

	download_from_flash(
		flash_offset,      /* The offset of the data in spi flash */
		CONFIG_PROGRAM_MEMORY_BASE, /* RAM Addr of downloaded data */
		flash_used,        /* Number of bytes to download      */
		SIGN_NO_CHECK,     /* Need CRC check or not               */
		addr_entry,        /* jump to this address after download */
		&status            /* Status fo download */
	);
}

void system_set_image_copy(enum ec_image copy)
{
	switch (copy) {
	case EC_IMAGE_RW:
		break;
#ifdef CONFIG_RW_B
	case EC_IMAGE_RW_B:
		break;
#endif
	default:
		/* Fall through to EC_IMAGE_RO */
	case EC_IMAGE_RO:
		break;
	}
}

uint32_t system_get_lfw_address(void)
{
	return (uint32_t)system_jump_to_booter;
}

const void *__image_size;
