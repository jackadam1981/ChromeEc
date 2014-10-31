/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "registers.h"
#include "system.h"

#ifndef CONFIG_COMMON_RUNTIME
/* RW firmware reset vector */
uint32_t * const rw_rst =
	(uint32_t *)(CONFIG_FLASH_BASE+CONFIG_FW_RW_OFF+4);
#endif

void pd_jump_to_rw(void)
{
#ifdef CONFIG_COMMON_RUNTIME
	system_run_image_copy(SYSTEM_IMAGE_RW);
#else
	void (*jump_rw_rst)(void) = (void *)*rw_rst;

	debug_printf("Jump to RW\n");
	/* Disable interrupts */
	asm volatile("cpsid i");
	/* Call RW firmware reset vector */
	jump_rw_rst();
#endif
}

int pd_is_ro_mode(void)
{
#ifdef CONFIG_COMMON_RUNTIME
	return system_get_image_copy() == SYSTEM_IMAGE_RO;
#else
	return (uint32_t)&pd_jump_to_rw < (uint32_t)rw_rst;
#endif
}
