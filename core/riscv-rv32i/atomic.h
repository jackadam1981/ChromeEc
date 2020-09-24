/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atomic operations for RISC_V */

#ifndef __CROS_EC_ATOMIC_H
#define __CROS_EC_ATOMIC_H

#include "common.h"
#include "cpu.h"
#include "task.h"

#define ATOMIC_OP(op, value, addr)             \
({                                             \
	uint32_t tmp;                          \
	asm volatile (                         \
		"amo" #op ".w.aqrl %0, %2, %1" \
		: "=r" (tmp), "+A" (*addr)     \
		: "r" (value));                \
	tmp;                                   \
})

static inline void deprecated_atomic_clear_bit(volatile uint32_t *addr,
					   uint32_t bits)
{
	ATOMIC_OP(and, ~bits, addr);
}

static inline void atomic_clear_bit(int *addr, int bits)
{
	ATOMIC_OP(and, ~bits, addr);
}

static inline void deprecated_atomic_or(volatile uint32_t *addr, uint32_t bits)
{
	ATOMIC_OP(or, bits, addr);
}

static inline int atomic_or(int *addr, int bits)
{
	ATOMIC_OP(or, bits, addr);
	 /* Since EC code does not use the return value,
	  * just return 0 not to perform unnecessary operations
	  */
	return 0;
}

static inline void deprecated_atomic_add(volatile uint32_t *addr,
					 uint32_t value)
{
	ATOMIC_OP(add, value, addr);
}

static inline int atomic_add(int *addr, int value)
{
	ATOMIC_OP(add, value, addr);
	 /* Since EC code does not use the return value,
	  * just return 0 not to perform unnecessary operations
	  */
	return 0;
}

static inline void deprecated_atomic_sub(volatile uint32_t *addr,
					 uint32_t value)
{
	ATOMIC_OP(add, -value, addr);
}

static inline int atomic_sub(int *addr, int value)
{
	ATOMIC_OP(add, -value, addr);
	 /* Since EC code does not use the return value,
	  * just return 0 not to perform unnecessary operations
	  */
	return 0;
}

static inline uint32_t deprecated_atomic_read_clear(volatile uint32_t *addr)
{
	return ATOMIC_OP(and, 0, addr);
}

static inline uint32_t deprecated_atomic_read_add(volatile uint32_t *addr,
					     uint32_t value)
{
	return ATOMIC_OP(add, value, addr);
}

static inline uint32_t deprecated_atomic_read_sub(volatile uint32_t *addr,
					     uint32_t value)
{
	return ATOMIC_OP(add, -value, addr);
}

#endif  /* __CROS_EC_ATOMIC_H */
