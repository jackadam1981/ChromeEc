/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC */

#include "common.h"
#include "flash.h"
#include "registers.h"

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_get_protect(int block)
{
	return !(STM32_FLASH_WRPR & (1 << block));
}

/* Get protect status from WRP option byte */
static int flash_physical_get_protect_wrp(int block)
{
	int reg = 0;
	int shift = 0;

	if (block < 16) {
		reg = REG32(STM32_OPTB_BASE + STM32_OPTB_WRP01);
		if (block < 8)
			shift = block;
		else
			shift = 8+block;
	} else {
#if CONFIG_FLASH_SIZE > 64 * 1024
		reg = REG32(STM32_OPTB_BASE + STM32_OPTB_WRP23);
		if (block < 24)
			shift = block-16;
		else
			shift = 8+(block-16);
#endif
	}

	return !(reg & (1 << shift)) && (reg & (1 << (shift+8)));
}

/*
 * TODO(crosbug.com/p/61671): This code copies a lot from
 * common/flash.c:flash_get_protect, is there a way to reuse that?
 */
uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;
	/* Region protection status: 0: RW, 1: RO */
	int protected_at_boot[2] = {1, 1};
	int protected_now[2] = {1, 1};
	int protected_at_boot_all = 1;
	int protected_now_all = 1;
	int i;

	/* Scan flash protection */
	for (i = 0; i < PHYSICAL_BANKS; i++) {
		/* Default: RW. */
		int region = 0;

		if (i >= WP_BANK_OFFSET &&
		    i < WP_BANK_OFFSET + WP_BANK_COUNT)
			region = 1;

		if (!flash_physical_get_protect(i)) {
			protected_now[region] = 0;
			protected_now_all = 0;
		}

		if (!flash_physical_get_protect_wrp(i)) {
			protected_at_boot[region] = 0;
			protected_at_boot_all = 0;
		}
	}

	if (protected_at_boot_all)
		flags |= EC_FLASH_PROTECT_ALL_AT_BOOT;
	if (protected_at_boot[1])
		flags |= EC_FLASH_PROTECT_RO_AT_BOOT;
	if (protected_now_all)
		flags |= EC_FLASH_PROTECT_ALL_NOW;
	if (protected_now[1])
		flags |= EC_FLASH_PROTECT_RO_NOW;

#ifdef CONFIG_FLASH_PROTECT_RW
	if (protected_at_boot[0])
		flags |= EC_FLASH_PROTECT_RW_AT_BOOT;

	if (protected_now[0])
		flags |= EC_FLASH_PROTECT_RW_NOW;
#endif

	return flags;
}

int flash_physical_protect_now(int all)
{
	return EC_ERROR_INVAL;
}

int flash_physical_restore_state(void)
{
	/* Nothing to restore */
	return 0;
}

uint32_t flash_physical_get_valid_flags(void)
{
	return EC_FLASH_PROTECT_RO_AT_BOOT |
	       EC_FLASH_PROTECT_RO_NOW |
#ifdef CONFIG_FLASH_PROTECT_RW
	       EC_FLASH_PROTECT_RW_AT_BOOT |
	       EC_FLASH_PROTECT_RW_NOW |
#endif
	       EC_FLASH_PROTECT_ALL_AT_BOOT |
	       EC_FLASH_PROTECT_ALL_NOW;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	uint32_t ret = 0;

	/* If RO protection isn't enabled, its at-boot state can be changed. */
	if (!(cur_flags & EC_FLASH_PROTECT_RO_NOW))
		ret |= EC_FLASH_PROTECT_RO_AT_BOOT;

	/*
	 * ALL/RW at-boot state can be set if WP GPIO is asserted and can always
	 * be cleared.
	 */
	if (cur_flags & (EC_FLASH_PROTECT_ALL_AT_BOOT |
			 EC_FLASH_PROTECT_GPIO_ASSERTED))
		ret |= EC_FLASH_PROTECT_ALL_AT_BOOT;

#ifdef CONFIG_FLASH_PROTECT_RW
	if (cur_flags & (EC_FLASH_PROTECT_RW_AT_BOOT |
			 EC_FLASH_PROTECT_GPIO_ASSERTED))
		ret |= EC_FLASH_PROTECT_RW_AT_BOOT;
#endif

	return ret;
}
