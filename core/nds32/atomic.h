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
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

static inline void atomic_or(uint32_t *addr, uint32_t bits)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

static inline void atomic_add(uint32_t *addr, uint32_t value)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

static inline void atomic_sub(uint32_t *addr, uint32_t value)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
}

static inline uint32_t atomic_read_clear(uint32_t *addr)
{
	/* TODO(crosbug.com/p/23574): IMPLEMENT ME ! */
	return 0;
}
#endif  /* __CROS_EC_ATOMIC_H */
