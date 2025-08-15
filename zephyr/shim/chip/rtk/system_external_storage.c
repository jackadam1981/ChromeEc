/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bbram.h"
#include "common.h"
#include "config_chip.h"
#include "system.h"
#include "system_chip.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>

#include <soc.h>

#ifdef CONFIG_CROS_EC_RO
static enum ec_image ec_RORW_type = EC_IMAGE_RO;
#elif CONFIG_CROS_EC_RW
static enum ec_image ec_RORW_type = EC_IMAGE_RW;
#else
#error "Unsupported image type!"
#endif

#define RTK_FW_LOAD_BASED CONFIG_CROS_EC_PROGRAM_MEMORY_BASE
#define RTK_FW_RESET_VECTOR (RTK_FW_LOAD_BASED + 0x4u)

void system_jump_to_booter(void)
{
	uint32_t flash_offset;
	uint32_t flash_used;

	__disable_irq();

	for (int i = 0; i < CONFIG_NUM_IRQS; i++) {
		NVIC_ClearPendingIRQ(i);
		NVIC_DisableIRQ(i);
	}

	/*
	 * Get memory offset and size for RO/RW regions.
	 */
	switch (system_get_shrspi_image_copy()) {
	case EC_IMAGE_RW:
		flash_offset =
			CONFIG_EC_WRITABLE_STORAGE_OFF + CONFIG_RW_STORAGE_OFF;
		flash_used = CONFIG_RW_SIZE;
		break;
	case EC_IMAGE_RO:
	default: /* Jump to RO by default */
		flash_offset =
			CONFIG_EC_PROTECTED_STORAGE_OFF + CONFIG_RO_STORAGE_OFF;
		flash_used = CONFIG_RO_SIZE;
		break;
	}

	/* RTK Read selected image from internal flash into SRAM
	 * Need a jump to little-fw (LFW).
	 */
	system_download_from_flash(flash_offset, RTK_FW_LOAD_BASED, flash_used,
				   CONFIG_CROS_EC_PROGRAM_MEMORY_BASE + 0x4u);
}

uint32_t system_get_lfw_address()
{
	uint32_t jump_addr = (uint32_t)system_jump_to_booter;
	return jump_addr;
}

enum ec_image system_get_shrspi_image_copy(void)
{
	return ec_RORW_type;
}

void system_set_image_copy(enum ec_image copy)
{
	uint32_t value = (uint32_t)copy;

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

	ec_RORW_type = value;
}
