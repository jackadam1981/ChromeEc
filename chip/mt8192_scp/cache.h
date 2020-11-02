/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CACHE_H
#define __CROS_EC_CACHE_H

#include "csr.h"
#include "stdint.h"

#define MPU_ATTR(B, C, W, R, P) \
	((B << CSR_MPU_ATTR_B) | \
	 (C << CSR_MPU_ATTR_C) | \
	 (W << CSR_MPU_ATTR_W) | \
	 (R << CSR_MPU_ATTR_R) | \
	 (P << CSR_MPU_ATTR_P))

struct mpu_entry {
	/* MPU_ATTR */
	unsigned int attribute;
	/* 1k alignment and the address is inclusive */
	unsigned int start_addr;
	/* 1k alignment in 8M_boundary and non-inclusive */
	unsigned int end_addr;
};

/* memory barrier of I$ */
void cache_barrier_icache(void);
/* invalidate all I$ */
void cache_invalidate_icache(void);
/* invalidate a range of I$ */
int cache_invalidate_icache_range(uintptr_t addr, uint32_t length);

/* memory barrier of D$ */
void cache_barrier_dcache(void);
/* writeback all D$ */
void cache_writeback_dcache(void);
/* writeback a range of D$ */
int cache_writebaack_dcache_range(uintptr_t addr, uint32_t length);
/* invalidate all D$ */
void cache_invalidate_dcache(void);
/* invalidate a range of D$ */
int cache_invalidate_dcache_range(uintptr_t addr, uint32_t length);
/* writeback and invalidate all D$ */
void cache_flush_dcache(void);
/* writeback and invalidate a range of D$ */
int cache_flush_dcache_range(uintptr_t addr, uint32_t length);

void cache_init(void);

#endif /* #ifndef __CROS_EC_CACHE_H */
