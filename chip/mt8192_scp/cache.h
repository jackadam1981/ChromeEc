/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CACHE_H
#define __CROS_EC_CACHE_H

#include "stdint.h"

void cache_invalidate_icache(void);
int cache_invalidate_icache_range(uintptr_t addr, uint32_t length);
void cache_invalidate_dcache(void);
int cache_invalidate_dcache_range(uintptr_t addr, uint32_t length);
void cache_writeback_dcache(void);
int cache_writebaack_dcache_range(uintptr_t addr, uint32_t length);
void cache_flush_dcache(void);
int cache_flush_dcache_range(uintptr_t addr, uint32_t length);

void cache_init(void);

#endif /* #ifndef __CROS_EC_CACHE_H */
