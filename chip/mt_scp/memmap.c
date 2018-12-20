/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SCP memory map
 */

#include "chip/mt_scp/registers.h"
#include "common.h"
#include "hooks.h"
#include "memmap.h"

struct addr_map {
	uintptr_t ap_addr;
	uintptr_t scp_addr;
};

/* This map should sync with the scp_memmap_init. */
static const struct addr_map addr_map[] = {
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

#define ADDR_MAP_MAX (sizeof(addr_map) / sizeof(struct addr_map))
#define ADDR_MSB_MASK 0xF0000000
#define ADDR_MASK 0x0FFFFFFF

void scp_memmap_init(void)
{
	/*
	 * SCP addr    :  AP addr
	 * 0xA0000000     0x10000000
	 * 0xB0000000     0x20000000
	 * 0xC0000000     0x30000000
	 * 0x20000000     0x40000000
	 * 0x30000000     0x50000000
	 * 0x60000000     0x60000000
	 * 0x70000000     0x70000000
	 * 0x80000000     0x80000000
	 * 0xF0000000     0x90000000
	 *
	 * Default config, LARGE DRAM not active:
	 *   REG32(0xA0001F00) & 0x2000 != 0
	 */

	/*
	 * SCP_REMAP_CFG1
	 * EXT_ADDR3[29:24] remap register for addr msb 31~28 equal to 0x7
	 * EXT_ADDR2[21:16] remap register for addr msb 31~28 equal to 0x6
	 * EXT_ADDR1[13:8]  remap register for addr msb 31~28 equal to 0x3
	 * EXT_ADDR0[5:0]   remap register for addr msb 31~28 equal to 0x2
	 */
	SCP_REMAP_CFG1 = 0x07060504;

	/*
	 * SCP_REMAP_CFG2
	 * EXT_ADDR7[29:24] remap register for addr msb 31~28 equal to 0xb
	 * EXT_ADDR6[21:16] remap register for addr msb 31~28 equal to 0xa
	 * EXT_ADDR5[13:8]  remap register for addr msb 31~28 equal to 0x9
	 * EXT_ADDR4[5:0]   remap register for addr msb 31~28 equal to 0x8
	 */
	SCP_REMAP_CFG2 = 0x02010008;
	/*
	 * SCP_REMAP_CFG3
	 * AUR_ADDR[31:28]  remap register for addr msb 31~28 equal to 0xd
	 * EXT_ADDR10[21:16]remap register for addr msb 31~28 equal to 0xf
	 * EXT_ADDR9[13:8]  remap register for addr msb 31~28 equal to 0xe
	 * EXT_ADDR8[5:0]   remap register for addr msb 31~28 equal to 0xc
	 */
	SCP_REMAP_CFG3 = 0x10000A03;
}

int memmap_ap_to_scp(uintptr_t ap_addr, uintptr_t *scp_addr)
{
	int i;

	for (i = 0; i < ADDR_MAP_MAX; i++) {
		if (addr_map[i].ap_addr != (ap_addr & ADDR_MSB_MASK))
			continue;

		*scp_addr = addr_map[i].scp_addr | (ap_addr & ADDR_MASK);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}

int memmap_scp_to_ap(uintptr_t scp_addr, uintptr_t *ap_addr)
{
	int i;

	for (i = 0; i < ADDR_MAP_MAX; i++) {
		if (addr_map[i].scp_addr != (scp_addr & ADDR_MSB_MASK))
			continue;

		*ap_addr = addr_map[i].ap_addr | (scp_addr & ADDR_MASK);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}

/*
 * AP addr     :  SCP cache addr
 * 0x50000000     0x10000000
 */
#define CACHE_TRANS_AP_ADDR 0x50000000
#define CACHE_TRANS_SCP_CACHE_ADDR 0x10000000

int memmap_ap_to_scp_cache(uintptr_t ap_addr, uintptr_t *scp_addr)
{
	if (CACHE_TRANS_AP_ADDR != (ADDR_MSB_MASK & ap_addr))
		return EC_ERROR_INVAL;

	*scp_addr = CACHE_TRANS_SCP_CACHE_ADDR | (ap_addr & ADDR_MASK);
	return EC_SUCCESS;
}

int memmap_scp_cache_to_ap(uintptr_t scp_addr, uintptr_t *ap_addr)
{
	if (CACHE_TRANS_SCP_CACHE_ADDR != (scp_addr & ADDR_MSB_MASK))
		return EC_ERROR_INVAL;

	*ap_addr = CACHE_TRANS_AP_ADDR | (scp_addr & ADDR_MASK);
	return EC_SUCCESS;
}
