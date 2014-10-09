/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Flash memory module for Chrome EC */

#include "common.h"
#include "flash.h"
#include "registers.h"

#define flash_restore_state() 0
#include "flash-f-f0.c"

/*****************************************************************************/
/* Physical layer APIs */

int flash_physical_get_protect(int block)
{
	return !(STM32_FLASH_WRPR & (1 << block));
}

uint32_t flash_physical_get_protect_flags(void)
{
	uint32_t flags = 0;

	if (STM32_FLASH_WRPR == 0)
		flags |= EC_FLASH_PROTECT_ALL_NOW;

	if (read_optb(STM32_OPTB_WRP_OFF(0)) == 0 &&
	    read_optb(STM32_OPTB_WRP_OFF(1)) == 0 &&
	    read_optb(STM32_OPTB_WRP_OFF(2)) == 0 &&
	    read_optb(STM32_OPTB_WRP_OFF(3)) == 0)
		flags |= EC_FLASH_PROTECT_ALL_AT_BOOT;

	return flags;
}

int flash_physical_protect_now(int all)
{
	return EC_ERROR_INVAL;
}
