/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H
#define __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H

#include "bool.h"
#include "stdint.h"

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
 * 'Read' a chunk of virtual NV memory.
 */
void _plat__NvVirtualMemoryRead(unsigned int startOffset,
                                unsigned int size,
                                void *data);

#endif /* __EC_BOARD_CR50_TPM2_VIRTUAL_NVMEM_H */
