/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H
#define __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H

#include "bool.h"
#include "stdint.h"

/*
 * Returns true if the handle falls with the range reserved for virtual NV
 * indexes; no writes should be attempted on handles in this range.
 */
BOOL _plat__NvHandleInVirtualRange(uint32_t handle);

/*
 * If the specified handle represents a virtual NV index, this function returns
 * the corresponding virtual offset. For all other handles, 0 is returned.
 */
uint32_t _plat__NvGetHandleVirtualOffset(uint32_t handle);

/*
 * Returns true iff the specified offset corresponds to virtual NV memory; if
 * so, this offset must only be read using the _plat__NvVirtualMemoryRead()
 * function below.
 */
BOOL _plat__NvOffsetIsVirtual(unsigned int startOffset);

/*
 * Equivalent of _plat__NvMemoryRead, but for virtual NV memory. Only a limited
 * number of offsets can be read using this function. For any value x returned
 * by _plat__NvGetHandleVirtualOffset, offsets x+4 and x+152 may be read using
 * this function. No other offsets may be read.
 */
void _plat__NvVirtualMemoryRead(unsigned int startOffset, unsigned int size,
				void *data);

#endif /* __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H */
