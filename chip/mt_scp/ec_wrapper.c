/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC wrapper
 */

#include "common.h"
#include "memmap.h"

uint32_t dma_ap_to_scp(uint32_t ap_addr) {
	uintptr_t scp_addr;

	if (EC_SUCCESS == memmap_ap_to_scp(ap_addr, &scp_addr))
		return scp_addr;

	return 0;
}

uint32_t dma_scp_to_ap(uint32_t scp_addr) {
	uintptr_t ap_addr;

	if (EC_SUCCESS == memmap_scp_to_ap(scp_addr, &ap_addr))
		return ap_addr;

	return 0;
}

uint32_t dma_scp_cache_to_ap(uint32_t scp_cache_addr) {
	uintptr_t ap_addr;

	if (EC_SUCCESS == memmap_scp_cache_to_ap(scp_cache_addr, &ap_addr))
		return ap_addr;

	return 0;
}

uint32_t dma_ap_to_scp_cache(uint32_t ap_addr) {
	uintptr_t scp_cache_addr;

	if (EC_SUCCESS == memmap_ap_to_scp_cache(ap_addr, &scp_cache_addr))
		return scp_cache_addr;
	
	return 0;
}
