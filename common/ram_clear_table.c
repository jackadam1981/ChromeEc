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

struct range {
	void *addr;
	size_t size;
};

const struct range ram_clear_table[] = {
	{.addr = (void *)CONFIG_RAM_BASE, .size = CONFIG_RAM_SIZE},
#ifdef CONFIG_CHIP_MEMORY_REGIONS
	/*
	 * We currently list all sections for clearing,
	 * irrespective of their permission.
	 */
#	define REGION(name, attr, start, size) \
	{(void *)(start), (size)},
#	include "memory_regions.inc"
#endif
};

const size_t ram_clear_table_size = sizeof(ram_clear_table);
