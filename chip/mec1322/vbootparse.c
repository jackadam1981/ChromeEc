/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "registers.h"
#include "spi_flash.h"
#include "spi.h"
#include "watchdog.h"

static uintptr_t *const Switch2RWImage =
		 (uintptr_t *const)SHARED_RAM_LOADER_RORW;

uint32_t vboot_hash_get_image_size(enum system_image_copy_t copy)
{

	uint8_t imagedata = 0;
	uint32_t size = CONFIG_FW_RW_SIZE;
	uint32_t image_addr = 0xFFFFFFFF;

	if (copy == SYSTEM_IMAGE_RW)
		image_addr = MEC1322_RW_IMAGE_FLASHADDR;
	else if (copy == SYSTEM_IMAGE_RO)
		image_addr = MEC1322_RO_IMAGE_FLASHADDR;

	if ((size <= 0) | (image_addr == 0xFFFFFFFFF))
		return 0;

	ccprintf("imagedata %x\n!!!", image_addr);

	/*
	 * Scan backwards looking for 0xea byte, which is by definition the
	 * last byte of the image.  See ec.lds.S for how this is inserted at
	 * the end of the image.
	 */
	for (size--; size > 0; size--) {
		watchdog_reload();
		spi_flash_read(&imagedata,
				(MEC1322_RW_IMAGE_FLASHADDR + size),
				 1);

		if (imagedata == 0xea)
			break;
	}
	return size ? size + 1 : 0;  /* 0xea byte IS part of the image */
}

void vboot_hash_get_next_chunk_addr(const uint8_t *vbootbuf ,
					uint32_t offset,
					uint32_t nbytes)
{
	spi_flash_read((uint8_t *)vbootbuf,
			 MEC1322_RW_IMAGE_FLASHADDR + offset,
			 nbytes);
}

void system_rw_jump(unsigned int enable)
{
	*Switch2RWImage = enable;
}


int system_run_image_loader(void)
{
	uintptr_t base;
	uintptr_t init_addr;

	/* Load the appropriate reset vector */
	base = CONFIG_FLASH_BASE + CONFIG_FW_LOADER_OFF;

	/* Make sure the reset vector is inside the destination image */
	init_addr = *(uintptr_t *)(base + 4);

	ccprintf("init addr %x\n", (uint32_t)init_addr);

	if (init_addr < base)
		return EC_ERROR_UNKNOWN;

	jump_to_image(init_addr);

	/* Should never get here */
	return EC_ERROR_UNKNOWN;
}

int system_run_image_copy_ext_spi(enum system_image_copy_t copy)
{
	if (copy == SYSTEM_IMAGE_RW)
		system_rw_jump(SYSTEM_IMAGE_RW);
	 else
		system_rw_jump(SYSTEM_IMAGE_RO);

	system_run_image_loader();

	/* Should never get here */
	return EC_ERROR_UNKNOWN;
}

static int command_imageswitch_2rw_enable(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* Handle named images */
	if (!strcasecmp(argv[1], "1"))
		system_rw_jump(SYSTEM_IMAGE_RW);
	else if (!strcasecmp(argv[1], "2")) {
		system_rw_jump(SYSTEM_IMAGE_RW);
		system_run_image_loader();
	} else
		return EC_ERROR_PARAM_COUNT;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(jump2rw, command_imageswitch_2rw_enable,
			"1 = enable switch ; 2 = switch  n jump",
			"Loader loads rw and switches",
			NULL);


