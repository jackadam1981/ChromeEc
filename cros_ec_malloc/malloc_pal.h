/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_MALLOC_PAL_H
#define __CROS_EC_MALLOC_PAL_H

#include "shared_mem.h"

/*
 * Acquires a shared memory area of the requested size in bytes.
 *
 * @param size	Number of bytes requested
 * @param data	If successful, set on return to the start of the
 *		granted memory buffer.
 *
 * @return 0 if successful, or other non-zero error code.
 */
static inline int32_t FpcMalloc(void **data, size_t size)
{
	return shared_mem_acquire(size, (char **)data);
}

/**
 * Releases a shared memory area previously allocated via FpcMalloc().
 */
static inline void FpcFree(void *ptr)
{
	return shared_mem_release(ptr);
}

void FpcMallocInit(void *chunk, size_t size);

#endif  /* __CROS_EC_MALLOC_PAL_H */
