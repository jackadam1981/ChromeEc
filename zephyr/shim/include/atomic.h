/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ATOMIC_H
#define __CROS_EC_ATOMIC_H

#include <sys/atomic.h>

/*
 * atomic_clear_bits: clears the requested mask bits.
 * atomic operations are supposed to return an atomic_val_t
 * to indicate if bits were set during the atomic operation
 * so this function will return that none were set.
 */
static inline atomic_val_t atomic_clear_bits(atomic_t *addr, atomic_val_t bits)
{
	(void)atomic_and(addr, ~bits);
	return 0;
}

#endif  /* __CROS_EC_ATOMIC_H */
