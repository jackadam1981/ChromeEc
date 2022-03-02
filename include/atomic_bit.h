/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This file is to provide atomic_t definition */

#ifndef __CROS_EC_ATOMIC_BIT_H
#define __CROS_EC_ATOMIC_BIT_H

#ifndef CONFIG_ZEPHYR
#include "atomic.h"
#include "stdbool.h"
#define ATOMIC_BITS (sizeof(atomic_val_t) * 8)
#define ATOMIC_MASK(bit) BIT((unsigned long)(bit) & (ATOMIC_BITS - 1U))
#define ATOMIC_ELEM(addr, bit) ((addr) + ((bit) / ATOMIC_BITS))

/**
 * This macro computes the number of atomic variables necessary to
 * represent a bitmap with num_bits.
 *
 * @param num_bits Number of bits.
 */
#define ATOMIC_BITMAP_SIZE(num_bits) (1 + ((num_bits) - 1) / ATOMIC_BITS)

/**
 * @brief Define an array of atomic variables.
 *
 * This macro defines an array of atomic variables containing at least
 * num_bits bits.
 *
 * @param name Name of array of atomic variables.
 * @param num_bits Number of bits needed.
 */
#define ATOMIC_DEFINE(name, num_bits) \
	atomic_t name[ATOMIC_BITMAP_SIZE(num_bits)]

/**
 * @brief Atomically test a bit.
 *
 * This routine tests whether bit of atomic variable is set or not.
 * The target may be a single atomic variable or an array of them.
 *
 * @param addr Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return true if the bit was set, false if it wasn't.
 */
static inline bool atomic_test_bit(const atomic_t *addr, int bit)
{
	atomic_val_t val = *ATOMIC_ELEM(addr, bit);

	return (1 & (val >> (bit & (ATOMIC_BITS - 1)))) != 0;
}

/**
 * @brief Atomically set a bit.
 *
 * Atomically set bit of atomic variable.
 * The target may be a single atomic variable or an array of them.
 *
 * @param addr Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 */
static inline void atomic_set_bit(atomic_t *addr, int bit)
{
	atomic_val_t mask = ATOMIC_MASK(bit);

	(void)atomic_or(ATOMIC_ELEM(addr, bit), mask);
}

/**
 * @brief Atomically clear a bit.
 *
 * Atomically clear bit of atomic variable.
 * The target may be a single atomic variable or an array of them.
 *
 * @param addr Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 */
static inline void atomic_clear_bit(atomic_t *addr, int bit)
{
	atomic_val_t mask = ATOMIC_MASK(bit);
	atomic_t *elem = ATOMIC_ELEM(addr, bit);

	*elem = *elem & ~mask;
}

#endif /* CONFIG_ZEPHYR */
#endif /* __CROS_EC_ATOMIC_BIT_H */
