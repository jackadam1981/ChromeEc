/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "flash.h"

void  __attribute__((section(".iram.text")))
iram_flash_write(uint32_t *addr, uint32_t *data)
{
}

int crec_flash_physical_write(int offset, int size, const char *data)
{
	return 0;
}

int crec_flash_physical_erase(int offset, int size)
{
	return 0;
}

int crec_flash_physical_get_protect(int block)
{
	return 0;
}

int crec_flash_physical_protect_at_boot(uint32_t new_flags)
{
	return 0;
}

int crec_flash_physical_force_reload(void)
{
	return 0;
}

uint32_t crec_flash_physical_get_protect_flags(void)
{
	return 0;
}

int crec_flash_physical_protect_now(int all)
{
	return 0;
}

uint32_t crec_flash_physical_get_valid_flags(void)
{
	return 0;
}

uint32_t crec_flash_physical_get_writable_flags(uint32_t cur_flags)
{
	return 0;
}

int crec_flash_pre_init(void)
{
	return 0;
}
