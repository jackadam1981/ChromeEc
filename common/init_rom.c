/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Init ROM module for Chrome EC */

#include "common.h"
#include "init_rom.h"
#include "flash.h"
#include "stdbool.h"
#include "stddef.h"

const uintptr_t *init_rom_get_addr(int offset, int size)
{
	const char *src;

	/*
	 * When flash isn't memory mapped, caller's must use init_rom_copy()
	 * to copy .init_rom data into RAM.
	 */
	if (!IS_ENABLED(CONFIG_MAPPED_STORAGE))
		return NULL;

	/*
	 * Convert flash offset to memory mapped address
	 */
	if (flash_dataptr(offset, size, 1, &src) < 0)
		return NULL;

	return (uintptr_t *)src;
}

void init_rom_lock(bool lock)
{
	flash_lock_mapped_storage(lock);
}

int init_rom_copy(int offset, int size, char *data)
{
	return flash_read(offset, size, data);
}

