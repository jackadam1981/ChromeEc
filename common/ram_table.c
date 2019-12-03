/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Holds the table for startup RAM clearing
 *
 * This table is used in init.S to clear all RAM on startup.
 */
#include <common.h>
#include <stddef.h>
#include "ram_table.h"

const struct mem_range ram_table[] = {
	{(void *)CONFIG_RAM_BASE, (void *)(CONFIG_RAM_BASE+CONFIG_RAM_SIZE)},
#ifdef CONFIG_CHIP_MEMORY_REGIONS
	/*
	 * We currently list all sections for clearing,
	 * irrespective of their permission.
	 */
#	define REGION(name, attr, start, size) \
		{(void *)(start), (void *)((start)+(size))},
#	include "memory_regions.inc"
#	undef REGION
#endif /* CONFIG_CHIP_MEMORY_REGIONS */
};

const size_t ram_table_size = sizeof(ram_table);

#ifdef CONFIG_CHIP_MEMORY_REGIONS
	/*
	* Ensure that the memory regions are word aligned.
	* This is required because init.S clears by word size.
	*/
#	define REGION(name, attr, start, size)                        \
		BUILD_ASSERT((start) % sizeof(void *) == 0);          \
		BUILD_ASSERT(((start)+(size)) % sizeof(void *) == 0);
#	include "memory_regions.inc"
#	undef REGION
#endif /* CONFIG_CHIP_MEMORY_REGIONS */
