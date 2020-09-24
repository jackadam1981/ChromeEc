/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atomic operations for emulator */

#ifndef __CROS_EC_ATOMIC_H
#define __CROS_EC_ATOMIC_H

#include "common.h"

static inline void deprecated_atomic_clear_bit(uint32_t volatile *addr,
					   uint32_t bits)
{
	__sync_and_and_fetch(addr, ~bits);
}

static inline void atomic_clear_bit(int *addr, int bits)
{
	__sync_and_and_fetch(addr, ~bits);
}

static inline void deprecated_atomic_or(uint32_t volatile *addr, uint32_t bits)
{
	__sync_or_and_fetch(addr, bits);
}

static inline int atomic_or(int *addr, int bits)
{
	__sync_or_and_fetch(addr, bits);
	 /* Since EC code does not use the return value,
	  * just return 0 not to perform unnecessary operations
	  */
	return 0;
}

static inline void deprecated_atomic_add(uint32_t volatile *addr,
					 uint32_t value)
{
	__sync_add_and_fetch(addr, value);
}

static inline int atomic_add(int *addr, int value)
{
	__sync_add_and_fetch(addr, value);
	 /* Since EC code does not use the return value just return 0
	  * to be compatible with other atomic_add implementations
	  */
	return 0;
}

static inline void deprecated_atomic_sub(uint32_t volatile *addr,
					 uint32_t value)
{
	__sync_sub_and_fetch(addr, value);
}

static inline int atomic_sub(int *addr, int value)
{
	__sync_sub_and_fetch(addr, value);
	 /* Since EC code does not use the return value just return 0
	  * to be compatible with other atomic_sub implementations
	  */
	return 0;
}

static inline uint32_t deprecated_atomic_read_clear(uint32_t volatile *addr)
{
	return __sync_fetch_and_and(addr, 0);
}
#endif  /* __CROS_EC_ATOMIC_H */
