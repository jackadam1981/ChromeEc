/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atomic operations for Andes */

#ifndef __CROS_EC_ATOMIC_H
#define __CROS_EC_ATOMIC_H

#include "common.h"

static inline void atomic_clear(uint32_t *addr, uint32_t bits)
{
	asm volatile ("setgie.d");
	*addr &= ~bits;
	asm volatile ("setgie.e");
}

static inline void atomic_or(uint32_t *addr, uint32_t bits)
{
	asm volatile ("setgie.d");
	*addr |= bits;
	asm volatile ("setgie.e");
}

static inline void atomic_add(uint32_t *addr, uint32_t value)
{
	asm volatile ("setgie.d");
	*addr += value;
	asm volatile ("setgie.e");
}

static inline void atomic_sub(uint32_t *addr, uint32_t value)
{
	asm volatile ("setgie.d");
	*addr -= value;
	asm volatile ("setgie.e");
}

static inline uint32_t atomic_read_clear(uint32_t *addr)
{
	uint32_t val;
	asm volatile ("setgie.d");
	val = *addr;
	*addr = 0;
	asm volatile ("setgie.e");
	return val;
}
#endif  /* __CROS_EC_ATOMIC_H */
