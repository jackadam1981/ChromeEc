/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chip/mt_scp/registers.h"
#include "common.h"
#include "dma_chip.h"
#include "hooks.h"

struct dma_addr_map {
	uint32_t ap_addr;
	uint32_t scp_addr;
};

/* This map should sync with the scp_memmap_init. */
static struct dma_addr_map addr_map[] = {
	{ .ap_addr = 0x40000000, .scp_addr = 0x20000000, },
	{ .ap_addr = 0x50000000, .scp_addr = 0x30000000, },
	{ .ap_addr = 0x60000000, .scp_addr = 0x60000000, },
	{ .ap_addr = 0x70000000, .scp_addr = 0x70000000, },
	{ .ap_addr = 0x80000000, .scp_addr = 0x80000000, },
	{ .ap_addr = 0x00000000, .scp_addr = 0x90000000, },
	{ .ap_addr = 0x10000000, .scp_addr = 0xA0000000, },
	{ .ap_addr = 0x20000000, .scp_addr = 0xB0000000, },
	{ .ap_addr = 0x30000000, .scp_addr = 0xC0000000, },
	{ .ap_addr = 0x90000000, .scp_addr = 0xF0000000, },
};

static struct dma_addr_map cache_addr_map[] = {
	{ .ap_addr = 0x50000000, .scp_addr = 0x10000000, },
};

#define ADDR_MAP_MAX (sizeof(addr_map) / sizeof(struct dma_addr_map))
#define CACHE_ADDR_MAP_MAX                                                     \
	(sizeof(cache_addr_map) / sizeof(struct dma_addr_map))
#define ADDR_MSB_MASK 0xF0000000
#define ADDR_MASK 0x0FFFFFFF

uint32_t dma_ap_to_scp(uint32_t ap_addr)
{
	int i;

	for (i = 0; i < ADDR_MAP_MAX; i++)
		if (addr_map[i].ap_addr == (ADDR_MSB_MASK & ap_addr))
			return addr_map[i].scp_addr | (ADDR_MASK & ap_addr);

	/* Error, cannot find mapped addr. */
	return 0;
}

uint32_t dma_scp_to_ap(uint32_t scp_addr)
{
	int i;

	for (i = 0; i < ADDR_MAP_MAX; i++)
		if (addr_map[i].scp_addr == (ADDR_MSB_MASK & scp_addr))
			return (addr_map[i].ap_addr | (ADDR_MASK & scp_addr));

	/* Error, cannot find mapped addr. */
	return 0;
}

uint32_t dma_ap_to_scp_cache(uint32_t ap_addr)
{
	int i;

	for (i = 0; i < CACHE_ADDR_MAP_MAX; i++)
		if (cache_addr_map[i].ap_addr == (ADDR_MSB_MASK & ap_addr))
			return (cache_addr_map[i].scp_addr |
				(ADDR_MASK & ap_addr));

	/* Error, cannot find mapped addr. */
	return 0;
}

uint32_t dma_scp_cache_to_ap(uint32_t scp_addr)
{
	int i;

	for (i = 0; i < CACHE_ADDR_MAP_MAX; i++)
		if (cache_addr_map[i].scp_addr == (ADDR_MSB_MASK & scp_addr))
			return (cache_addr_map[i].ap_addr |
				(ADDR_MASK & scp_addr));

	/* Error, cannot find mapped addr. */
	return 0;
}
